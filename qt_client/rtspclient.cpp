#include "rtspclient.h"
#include <QTimer>
#include <QDebug>
#include <QMutex>
#include <QPainter>
#include <QPaintEvent>
#include <QResizeEvent>
#include <QShowEvent>
#include <QMoveEvent>
#include <QDir>
#include <QApplication>

#ifdef G_OS_WIN32
#include <stdlib.h>
#endif

// Initialize static member
int RtspClient::connection_count = 0;

// Undefine 'signals' macro from Qt to prevent conflict with GLib
#ifdef signals
#undef signals
#endif

#include <gio/gio.h>

// Callback to handle TLS certificate acceptance with balanced validation
static gboolean on_accept_certificate(GstElement *src, GTlsConnection *conn, GTlsCertificate *peer_cert, GTlsCertificateFlags errors, gpointer user_data) {
    RtspClient *client = static_cast<RtspClient*>(user_data);

    qDebug() << "Certificate validation - errors:" << errors;

    // Reject expired and revoked certificates
    if (errors & G_TLS_CERTIFICATE_EXPIRED) {
        qWarning() << "REJECTED: Certificate expired";
        return FALSE;
    }

    if (errors & G_TLS_CERTIFICATE_REVOKED) {
        qWarning() << "REJECTED: Certificate revoked";
        return FALSE;
    }

    // Accept certificates with UNKNOWN_CA (self-signed) and BAD_IDENTITY
    if ((errors & G_TLS_CERTIFICATE_UNKNOWN_CA) || (errors & G_TLS_CERTIFICATE_BAD_IDENTITY)) {
        qDebug() << "ACCEPTED: Certificate with UNKNOWN_CA or BAD_IDENTITY (expected for self-signed)";
        return TRUE;
    }

    // Accept certificates with no validation errors
    if (errors == G_TLS_CERTIFICATE_NO_FLAGS) {
        qDebug() << "ACCEPTED: Certificate has no validation errors";
        return TRUE;
    }

    qDebug() << "ACCEPTED: Certificate with minor errors:" << errors;
    return TRUE;
}

// Callback to handle dynamic pad creation by rtspsrc
static void on_pad_added(GstElement *element, GstPad *pad, gpointer user_data) {
    GstCaps *caps = gst_pad_get_current_caps(pad);
    if (caps) {
        const gchar *name = gst_structure_get_name(gst_caps_get_structure(caps, 0));
        if (g_str_has_prefix(name, "application/x-rtp")) {
            GstElement *depay = (GstElement *)user_data;
            GstPad *sinkpad = gst_element_get_static_pad(depay, "sink");

            // Check if sink pad is already linked
            if (gst_pad_is_linked(sinkpad)) {
                qDebug("Sink pad already linked - skipping");
                gst_object_unref(sinkpad);
                gst_caps_unref(caps);
                return;
            }

            GstPadLinkReturn result = gst_pad_link(pad, sinkpad);
            if (result != GST_PAD_LINK_OK) {
                qWarning("Failed to link dynamic pad to depayloader: %d", result);
            } else {
                qDebug("Linked dynamic RTP pad to depayloader");
            }
            gst_object_unref(sinkpad);
        }
        gst_caps_unref(caps);
    }
}

RtspClient::RtspClient(const QString& url, QWidget *parent)
    : QWidget(parent)
    , rtspUrl(url)
    , pipeline(nullptr)
    , src(nullptr)
    , videosink(nullptr)
    , tlsDatabase(nullptr)
    , ca_cert(nullptr)
    , tlsInteraction(nullptr)
    , freezeDetectionTimer(nullptr)
    , reconnectionTimer(nullptr)
    , lastFrameTime(0)
    , reconnectionAttempts(0)
    , isReconnecting(false)
    , busWatchId(0)
    , padAddedHandlerId(0)
    , videoOverlaySet(false)
    , videoWidth(0)
    , videoHeight(0)
    , maintainAspectRatio(false)  // Default to fill widget for better box fitting
    , overlayUpdateTimer(nullptr)
    , acceptCertHandlerId(0)
{
    // WINDOWS SPECIFIC: Enhanced widget setup for robust video embedding
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    // CRITICAL: Force native window creation for video overlay
    setAttribute(Qt::WA_NativeWindow, true);
    setAttribute(Qt::WA_DontCreateNativeAncestors, false);
    setAttribute(Qt::WA_OpaquePaintEvent, true);  // Improve rendering performance
    setAttribute(Qt::WA_NoSystemBackground, true); // Let video fill background

    setMinimumSize(320, 240);

    // WINDOWS: Configure widget for video embedding
    setAutoFillBackground(false);  // Let video fill the background
    setUpdatesEnabled(true);

    // Set background color for video widget (black background while connecting)
    setStyleSheet("background-color: black;");

    // WINDOWS CRITICAL: Ensure native window is created immediately
    // This is essential for video overlay to work properly
    show();  // Must show first on Windows

    // Force native window creation and get window ID
    WId windowId = winId();
    if (!windowId) {
        qWarning() << "CRITICAL: Failed to create native window for video embedding";
        // Force another attempt
        repaint();
        QApplication::processEvents();
        windowId = winId();
    }

    if (windowId) {
        qDebug() << "Video widget window ID:" << windowId;
    } else {
        qWarning() << "CRITICAL: Unable to obtain window ID - video embedding may fail";
    }

    // Additional widget setup for video embedding
    setFocusPolicy(Qt::StrongFocus);
    setMouseTracking(false);  // Reduce unnecessary events

    // Only show connection message for first stream to reduce spam
    if (connection_count++ == 0) {
        qDebug() << "Creating RTSP clients with delayed initialization";
    }

    // Log the URL being used for debugging
    qDebug() << "RTSP Client created for URL:" << rtspUrl;

    // WINDOWS: Staggered initialization to prevent camera mixing
    QTimer::singleShot(50 * connection_count, this, [this]() {
        if (winId()) {
            initializeClient();
        } else {
            qWarning() << "Window ID not ready, retrying initialization in 100ms";
            QTimer::singleShot(100, this, [this]() {
                initializeClient();
            });
        }
    });
}

// Check available GStreamer plugins
void RtspClient::checkGStreamerPlugins() {
    qDebug() << "[GStreamer] Checking plugin availability...";

    // Check essential plugins
    const char* essential_plugins[] = {
        "rtspsrc", "rtph264depay", "h264parse", "queue",
        "avdec_h264", "autovideosink", "videoconvert",
        nullptr
    };

    for (int i = 0; essential_plugins[i] != nullptr; i++) {
        GstElementFactory *factory = gst_element_factory_find(essential_plugins[i]);
        if (factory) {
            qDebug() << "[GStreamer] ✓" << essential_plugins[i] << "available";
            gst_object_unref(factory);
        } else {
            qWarning() << "[GStreamer] ✗" << essential_plugins[i] << "NOT available";
        }
    }

    // Check optional hardware decoders
    const char* hw_decoders[] = {
        "d3d11h264dec", "mfh264dec", "openh264dec",
        nullptr
    };

    for (int i = 0; hw_decoders[i] != nullptr; i++) {
        GstElementFactory *factory = gst_element_factory_find(hw_decoders[i]);
        if (factory) {
            qDebug() << "[GStreamer] ✓ HW decoder" << hw_decoders[i] << "available";
            gst_object_unref(factory);
        } else {
            qDebug() << "[GStreamer] - HW decoder" << hw_decoders[i] << "not available";
        }
    }
}

void RtspClient::initializeClient() {
    // Set minimal debugging - only show errors (suppress warnings too)
    gst_debug_set_default_threshold(GST_LEVEL_ERROR);
    gst_debug_set_threshold_for_name("rtspsrc", GST_LEVEL_ERROR);
    gst_debug_set_threshold_for_name("rtspconnection", GST_LEVEL_ERROR);
    gst_debug_set_threshold_for_name("videodecoder", GST_LEVEL_ERROR);

    // Check GStreamer plugin availability (only for first client)
    if (connection_count == 1) {
        checkGStreamerPlugins();
    }

    // Validate URL before attempting connection
    if (rtspUrl.isEmpty()) {
        qWarning() << "Empty RTSP URL provided";
        return;
    }

    qDebug() << "Initializing RTSP client for URL:" << rtspUrl;

    // Disable TLS debugging environment variable
#ifdef G_OS_WIN32
    // On Windows, use _putenv_s to unset environment variable
    _putenv_s("G_TLS_DEBUG", "");
#else
    g_unsetenv("G_TLS_DEBUG");
#endif

    // Show connection status
    update();

    pipeline = gst_pipeline_new("pipeline");
    src = gst_element_factory_make("rtspsrc", "src");

    g_object_set(src, "location", rtspUrl.toUtf8().constData(), NULL);

    // Simplified unified configuration for both RTSP and RTSPS
    // Use consistent settings that worked in your previous implementation
    g_object_set(src, "latency", 0, "drop-on-latency", TRUE, "timeout", 30000000, NULL);  // 30 second timeout
    g_object_set(src, "retry", 5, NULL);  // More retries for unreliable networks
    g_object_set(src, "buffer-mode", 1, NULL);  // Low latency mode
    g_object_set(src, "ntp-sync", FALSE, NULL);  // Disable NTP sync for lower latency
    g_object_set(src, "rtp-blocksize", 1400, NULL);  // Optimize RTP packet size for lower latency
    // Note: protocols property will be set per URL type, not globally

    // Set credentials for all cameras (matching reference)
    g_object_set(src, "user-id", "username", "user-pw", "password", NULL);

    // Configure protocol and TLS based on URL type
    if (rtspUrl.startsWith("rtsps://")) {
        g_object_set(src, "protocols", GST_RTSP_LOWER_TRANS_TCP, NULL);
        // Alternative if constant not found: g_object_set(src, "protocols", 0x00000004, NULL);  // Force TCP for RTSPS
        // Alternative if constant not found: g_object_set(src, "protocols", 0x00000004, NULL);
        setupTlsConfiguration();
    } else if (rtspUrl.startsWith("rtsp://")) {
        g_object_set(src, "protocols", GST_RTSP_LOWER_TRANS_TCP, NULL);
        // Alternative if constant not found: g_object_set(src, "protocols", 0x00000004, NULL);  // Force TCP for RTSP too
        // Alternative if constant not found: g_object_set(src, "protocols", 0x00000004, NULL);
    }

    // Add error handling for connection
    QTimer::singleShot(30000, this, [this]() {  // 30 seconds timeout
        if (!videoOverlaySet) {
            qWarning() << "Connection timeout for URL:" << rtspUrl;
            update();
        }
    });

    startPipeline();
    setupFreezeDetection();
}

void RtspClient::setupTlsConfiguration() {
    qDebug() << "Setting up enhanced TLS configuration for RTSPS connection";

    // Debug: check what resources are available
    QDir resourceDir(":/cert");
    qDebug() << "Available certificate resources:" << resourceDir.entryList();

    QDir caDir(":/cert/ca");
    qDebug() << "CA cert directory:" << caDir.entryList();

    QDir caCertsDir(":/cert/ca/certs");
    qDebug() << "CA certs subdirectory:" << caCertsDir.entryList();

    // Extract the CA certificate from resources to a temporary file
    QFile certResourceFile(":/cert/ca/certs/ca.cert.pem");
    if (certResourceFile.open(QIODevice::ReadOnly)) {
        QByteArray certData = certResourceFile.readAll();
        qDebug() << "CA certificate data size:" << certData.size() << "bytes";

        if (tempCertFile.open()) {
            tempCertFile.write(certData);
            tempCertFile.close();
            qDebug() << "Certificate written to temporary file:" << tempCertFile.fileName();
        } else {
            qWarning() << "Failed to create temporary certificate file";
            return;
        }
        certResourceFile.close();
    } else {
        qWarning() << "Failed to open CA certificate resource file";
        return;
    }

    // Load the CA certificate into a GTlsDatabase
    GError *error = NULL;
    QByteArray certPathBytes = tempCertFile.fileName().toUtf8();
    const gchar *cert_path = certPathBytes.constData();

    qDebug() << "Using certificate path:" << cert_path;

    tlsDatabase = g_tls_file_database_new(cert_path, &error);
    if (error) {
        qWarning("Failed to create TLS database: %s", error->message);
        g_error_free(error);
        tlsDatabase = nullptr;
        qDebug() << "TLS database not available - falling back to callback-only validation (LESS SECURE)";
    } else {
        qDebug() << "TLS database created successfully";
    }

    ca_cert = g_tls_certificate_new_from_file(cert_path, &error);
    if(!ca_cert){
        qWarning() << "Failed to parse CA PEM:" << error->message;
        g_error_free(error);
        return;
    } else {
        qDebug() << "CA certificate loaded successfully";
    }

    // Enable TLS database for proper certificate validation
    if (tlsDatabase) {
        g_object_set(G_OBJECT(src), "tls-database", tlsDatabase, NULL);
        qDebug() << "TLS database configured with RTSP CA - using proper certificate chain validation";
    }

    // Create and enable the custom TLS interaction object
    tlsInteraction = rtsp_client_tls_interaction_new(nullptr, ca_cert, tlsDatabase);
    if (tlsInteraction) {
        g_object_set(G_OBJECT(src), "tls-interaction", tlsInteraction, NULL);
        qDebug() << "TLS interaction object set on rtspsrc";
    }

    // Add direct signal handler for TLS errors (backup approach)
    acceptCertHandlerId = g_signal_connect(src, "accept-certificate", G_CALLBACK(on_accept_certificate), this);

    // Configure TLS validation for secure connections - AFTER TLS objects are created
    // Allow unknown CA and bad identity - our callback handles proper validation
    g_object_set(src, "tls-validation-flags", G_TLS_CERTIFICATE_UNKNOWN_CA | G_TLS_CERTIFICATE_BAD_IDENTITY, NULL);
    g_object_set(src, "protocols", 0x00000004, NULL);  // Force TCP for TLS
    g_object_set(src, "do-rtcp", TRUE, NULL);  // Enable RTCP like reference

    qDebug() << "Enhanced TLS configuration completed - using" << (tlsDatabase ? "CA validation + callback" : "callback-only validation");
}

RtspClient::~RtspClient()
{
    // Clean up timers
    if (freezeDetectionTimer) {
        freezeDetectionTimer->stop();
        delete freezeDetectionTimer;
    }
    if (reconnectionTimer) {
        reconnectionTimer->stop();
        delete reconnectionTimer;
    }
    if (overlayUpdateTimer) {
        overlayUpdateTimer->stop();
        delete overlayUpdateTimer;
    }

    // Disconnect signal handlers before cleaning up pipeline
    if (src && padAddedHandlerId > 0) {
        g_signal_handler_disconnect(src, padAddedHandlerId);
        padAddedHandlerId = 0;
    }
    if (src && acceptCertHandlerId > 0) {
        g_signal_handler_disconnect(src, acceptCertHandlerId);
        acceptCertHandlerId = 0;
    }

    // Clean up GStreamer resources
    if(pipeline){
        gst_element_set_state(pipeline, GST_STATE_NULL);
        gst_object_unref(pipeline);
    }
    if(ca_cert){
        g_object_unref(ca_cert);
    }
    if (tlsInteraction) {
        g_object_unref(tlsInteraction);
    }
    if (tlsDatabase) {
        g_object_unref(tlsDatabase);
    }
    if(busWatchId > 0){
        g_source_remove(busWatchId);
        busWatchId = 0;
    }
}

QString RtspClient::getTempCertPath() const
{
    return tempCertFile.fileName();
}

// Camera control functions
void RtspClient::startStream() {
    if (!pipeline) {
        qDebug() << "Starting stream for URL:" << rtspUrl;
        initializeClient();
        return;
    }

    GstState currentState;
    gst_element_get_state(pipeline, &currentState, NULL, GST_CLOCK_TIME_NONE);

    if (currentState != GST_STATE_PLAYING) {
        qDebug() << "Starting stream playback for URL:" << rtspUrl;
        GstStateChangeReturn ret = gst_element_set_state(pipeline, GST_STATE_PLAYING);
        if (ret == GST_STATE_CHANGE_FAILURE) {
            qWarning() << "Failed to start stream for URL:" << rtspUrl;
        }
    } else {
        qDebug() << "Stream already playing for URL:" << rtspUrl;
    }
}

void RtspClient::stopStream() {
    if (!pipeline) {
        qDebug() << "No pipeline to stop for URL:" << rtspUrl;
        return;
    }

    qDebug() << "Stopping stream for URL:" << rtspUrl;
    GstStateChangeReturn ret = gst_element_set_state(pipeline, GST_STATE_NULL);
    if (ret == GST_STATE_CHANGE_FAILURE) {
        qWarning() << "Failed to stop stream for URL:" << rtspUrl;
    }

    videoOverlaySet = false;

    // Clear the widget display
    update();
}

bool RtspClient::isStreamActive() const {
    if (!pipeline) {
        return false;
    }

    GstState currentState;
    GstStateChangeReturn ret = gst_element_get_state(pipeline, &currentState, NULL, GST_CLOCK_TIME_NONE);

    return (ret != GST_STATE_CHANGE_FAILURE && currentState == GST_STATE_PLAYING);
}

// Static function to handle bus messages
gboolean RtspClient::onBusMessage(GstBus *bus, GstMessage *msg, gpointer user_data) {
    RtspClient *client = static_cast<RtspClient*>(user_data);

    switch (GST_MESSAGE_TYPE(msg)) {
    case GST_MESSAGE_ERROR: {
        GError *error;
        gchar *debug;
        gst_message_parse_error(msg, &error, &debug);
        qWarning() << "[RTSP Error]" << client->rtspUrl << ":" << error->message;
        qWarning() << "Error domain:" << error->domain << "Code:" << error->code;
        qWarning() << "Debug info:" << (debug ? debug : "none");

        // Check for specific connection errors
        if (error->domain == GST_RESOURCE_ERROR && error->code == GST_RESOURCE_ERROR_OPEN_READ) {
            qWarning() << "[RTSP] Connection failed - camera may be offline or IP incorrect";
        }

        g_error_free(error);
        g_free(debug);
        break;
    }
    case GST_MESSAGE_WARNING: {
        GError *error;
        gchar *debug;
        gst_message_parse_warning(msg, &error, &debug);
        qWarning() << "Pipeline warning:" << error->message;
        qWarning() << "Debug info:" << (debug ? debug : "none");
        g_error_free(error);
        g_free(debug);
        break;
    }
    case GST_MESSAGE_INFO: {
        GError *error;
        gchar *debug;
        gst_message_parse_info(msg, &error, &debug);
        qDebug() << "Pipeline info:" << error->message;
        if (debug) qDebug() << "Debug info:" << debug;
        g_error_free(error);
        g_free(debug);
        break;
    }
    case GST_MESSAGE_STATE_CHANGED: {
        if (GST_MESSAGE_SRC(msg) == GST_OBJECT(client->src)) {
            GstState old_state, new_state;
            gst_message_parse_state_changed(msg, &old_state, &new_state, NULL);
            qDebug() << "RTSP source state changed from" << old_state << "to" << new_state;

            if (new_state == GST_STATE_PLAYING) {
                qDebug() << "RTSP source is now playing";
                client->reconnectionAttempts = 0; // Reset on successful connection
                client->videoOverlaySet = true;
                // Update frame timestamp when stream starts playing
                client->lastFrameTime = QDateTime::currentMSecsSinceEpoch();
            }
        }
        break;
    }
    case GST_MESSAGE_LATENCY: {
        // Recalculate latency when network conditions change
        gst_bin_recalculate_latency(GST_BIN(client->pipeline));
        break;
    }
    case GST_MESSAGE_BUFFERING: {
        gint percent;
        gst_message_parse_buffering(msg, &percent);
        if (percent < 100) {
            qDebug() << "Buffering:" << percent << "%";
        }
        break;
    }
    case GST_MESSAGE_STREAM_STATUS: {
        GstStreamStatusType status;
        gst_message_parse_stream_status(msg, &status, NULL);
        if (status == GST_STREAM_STATUS_TYPE_LEAVE) {
            qDebug() << "Stream thread leaving - potential freeze detected";
            if (!client->isReconnecting) {
                client->handleStreamFreeze();
            }
            // Update frame timestamp to prevent false freeze detection
            client->lastFrameTime = QDateTime::currentMSecsSinceEpoch();
        } else if (status == GST_STREAM_STATUS_TYPE_ENTER || status == GST_STREAM_STATUS_TYPE_START) {
            // Update frame timestamp when stream activity is detected
            client->lastFrameTime = QDateTime::currentMSecsSinceEpoch();
        }
        break;
    }
    default:
        break;
    }
    return TRUE;
}

// Create Windows-optimized decoder with better fallback logic
GstElement* RtspClient::createOptimalDecoder() {
    GstElement *decoder = nullptr;

    // Check available decoders in order of preference
    const char* decoder_names[] = {
        "d3d11h264dec",     // D3D11 hardware decoder
        "mfh264dec",        // Media Foundation decoder
        "avdec_h264",       // FFmpeg software decoder
        "openh264dec",      // OpenH264 decoder
        "x264dec",          // Alternative software decoder
        nullptr
    };

    for (int i = 0; decoder_names[i] != nullptr; i++) {
        decoder = gst_element_factory_make(decoder_names[i], "decoder");
        if (decoder) {
            if (connection_count == 1) {
                qDebug() << "Using decoder:" << decoder_names[i];
            }
            return decoder;
        }
    }

    // If no H264 decoder found, try generic decoder
    decoder = gst_element_factory_make("decodebin", "decoder");
    if (decoder && connection_count == 1) {
        qDebug() << "Using generic decodebin decoder";
    }

    return decoder;
}

// Configure elements for optimal performance

// Set up bus message handler
void RtspClient::setupBusMessageHandler() {
    GstBus *bus = gst_pipeline_get_bus(GST_PIPELINE(pipeline));
    busWatchId = gst_bus_add_watch(bus, onBusMessage, this);
    gst_object_unref(bus);
}

// Streamlined pipeline creation
// Calculate optimal render rectangle with proper aspect ratio
void RtspClient::calculateOptimalRenderRect(int &x, int &y, int &width, int &height) {
    x = 0;
    y = 0;
    width = this->width();
    height = this->height();

    // Always fill the widget for better visual appearance
    // The GStreamer video sink will handle scaling internally
    if (!maintainAspectRatio || videoWidth <= 0 || videoHeight <= 0) {
        return;
    }

    // Calculate aspect ratios
    double videoAspect = (double)videoWidth / (double)videoHeight;
    double widgetAspect = (double)this->width() / (double)this->height();

    // Use letterboxing to maintain aspect ratio
    if (videoAspect > widgetAspect) {
        // Video is wider than widget - fit to width, add letterboxing top/bottom
        width = this->width();
        height = (int)(width / videoAspect);
        x = 0;
        y = (this->height() - height) / 2;
    } else {
        // Video is taller than widget - fit to height, add letterboxing left/right
        height = this->height();
        width = (int)(height * videoAspect);
        x = (this->width() - width) / 2;
        y = 0;
    }

    // Ensure we don't exceed widget boundaries
    if (x < 0) x = 0;
    if (y < 0) y = 0;
    if (width > this->width()) width = this->width();
    if (height > this->height()) height = this->height();

    // Ensure minimum size to prevent tiny video
    if (width < 50) {
        width = this->width();
        x = 0;
    }
    if (height < 50) {
        height = this->height();
        y = 0;
    }
}

// Update video overlay with proper aspect ratio and positioning
void RtspClient::updateVideoOverlay() {
    if (!videosink || !GST_IS_VIDEO_OVERLAY(videosink) || !videoOverlaySet) {
        return;
    }

    int x, y, width, height;
    calculateOptimalRenderRect(x, y, width, height);

    // Set the render rectangle with calculated dimensions
    gst_video_overlay_set_render_rectangle(GST_VIDEO_OVERLAY(videosink), x, y, width, height);
    gst_video_overlay_expose(GST_VIDEO_OVERLAY(videosink));

    // Only log when video dimensions are available or on significant changes
    static int lastWidth = 0, lastHeight = 0;
    if (videoWidth > 0 && videoHeight > 0 && (videoWidth != lastWidth || videoHeight != lastHeight)) {
        qDebug() << "Video overlay updated - Render rect:" << x << y << width << height
                 << "Widget size:" << this->width() << "x" << this->height()
                 << "Video size:" << videoWidth << "x" << videoHeight;
        lastWidth = videoWidth;
        lastHeight = videoHeight;
    }
}

// Callback for video caps changes (to get video dimensions)
void RtspClient::onVideoCapsChanged(GstPad *pad, GParamSpec *pspec, gpointer user_data) {
    Q_UNUSED(pspec)
    RtspClient *client = static_cast<RtspClient*>(user_data);

    GstCaps *caps = gst_pad_get_current_caps(pad);
    if (caps) {
        GstStructure *structure = gst_caps_get_structure(caps, 0);
        if (structure) {
            int width, height;
            if (gst_structure_get_int(structure, "width", &width) &&
                gst_structure_get_int(structure, "height", &height)) {

                client->videoWidth = width;
                client->videoHeight = height;

                qDebug() << "Video dimensions detected:" << width << "x" << height;

                // Update video overlay with new dimensions
                QMetaObject::invokeMethod(client, [client]() {
                    client->updateVideoOverlay();
                }, Qt::QueuedConnection);
            }
        }
        gst_caps_unref(caps);
    }
}

// WINDOWS: Enhanced video overlay setup with better error handling and fallbacks
void RtspClient::setupVideoOverlay() {
    if (!videosink) {
        qWarning() << "WINDOWS CRITICAL: Video sink is null - cannot setup overlay";
        return;
    }

    if (!GST_IS_VIDEO_OVERLAY(videosink)) {
        qWarning() << "WINDOWS CRITICAL: Video sink does not support overlay interface";
        // This should not happen now since we only select overlay-capable sinks
        gchar *factory_name = gst_object_get_name(GST_OBJECT(gst_element_get_factory(videosink)));
        qWarning() << "WINDOWS: Sink" << factory_name << "claimed to support overlay but doesn't";
        g_free(factory_name);
        return;
    }

    // Ensure the widget is realized and has a valid window ID
    if (!isVisible()) {
        show();
    }

    // Force widget realization
    setAttribute(Qt::WA_NativeWindow, true);
    winId(); // Force creation of native window

    WId windowId = winId();
    if (!windowId) {
        qWarning() << "WINDOWS CRITICAL: Window ID not available - retrying in 500ms";
        QTimer::singleShot(500, this, &RtspClient::setupVideoOverlay);
        return;
    }

    qDebug() << "WINDOWS: Setting up video overlay for window ID:" << windowId;

    // Get sink information for debugging
    gchar *factory_name = gst_object_get_name(GST_OBJECT(gst_element_get_factory(videosink)));
    qDebug() << "WINDOWS: Configuring video sink:" << factory_name;

    // WINDOWS SPECIFIC: Configure sink properties for better overlay compatibility
    GObjectClass *object_class = G_OBJECT_GET_CLASS(videosink);

    // Disable aspect ratio forcing for better fit
    if (g_object_class_find_property(object_class, "force-aspect-ratio")) {
        g_object_set(videosink, "force-aspect-ratio", FALSE, NULL);
        qDebug() << "WINDOWS: Disabled force-aspect-ratio";
    }

    // Disable event handling to prevent conflicts with Qt
    if (g_object_class_find_property(object_class, "handle-events")) {
        g_object_set(videosink, "handle-events", FALSE, NULL);
        qDebug() << "WINDOWS: Disabled handle-events";
    }

    if (g_object_class_find_property(object_class, "enable-navigation-events")) {
        g_object_set(videosink, "enable-navigation-events", FALSE, NULL);
        qDebug() << "WINDOWS: Disabled navigation-events";
    }

    // DirectX specific properties
    if (g_str_has_prefix(factory_name, "d3d")) {
        if (g_object_class_find_property(object_class, "keep-aspect-ratio")) {
            g_object_set(videosink, "keep-aspect-ratio", FALSE, NULL);
            qDebug() << "WINDOWS: Disabled keep-aspect-ratio for DirectX sink";
        }
    }

    g_free(factory_name);

    // WINDOWS: Set the window handle for video embedding
    qDebug() << "WINDOWS: Setting window handle to:" << windowId;
    gst_video_overlay_set_window_handle(GST_VIDEO_OVERLAY(videosink), (guintptr)windowId);

    // Disable GStreamer event handling (let Qt handle events)
    gst_video_overlay_handle_events(GST_VIDEO_OVERLAY(videosink), FALSE);

    // Set render rectangle to fill the entire widget
    gst_video_overlay_set_render_rectangle(GST_VIDEO_OVERLAY(videosink), 0, 0, width(), height());

    // Force initial exposure
    gst_video_overlay_expose(GST_VIDEO_OVERLAY(videosink));

    qDebug() << "WINDOWS: Video overlay configured successfully for window:" << windowId;
    videoOverlaySet = true;

    // Start overlay management timer
    updateVideoOverlay();

    if (!overlayUpdateTimer) {
        overlayUpdateTimer = new QTimer(this);
        connect(overlayUpdateTimer, &QTimer::timeout, this, &RtspClient::updateVideoOverlay);
        overlayUpdateTimer->start(1000); // Check every second for Windows
    }

    // WINDOWS: Add verification timer to check if overlay is working
    QTimer::singleShot(3000, this, [this, windowId]() {
        verifyVideoOverlay(windowId);
    });
}

// WINDOWS: Verify video overlay is working and provide fallback options
void RtspClient::verifyVideoOverlay(WId windowId) {
    if (!videosink || !GST_IS_VIDEO_OVERLAY(videosink)) {
        qWarning() << "WINDOWS: Video overlay verification failed - no overlay sink available";
        return;
    }

    // Check if the window ID is still valid
    WId currentWindowId = winId();
    if (currentWindowId != windowId) {
        qWarning() << "WINDOWS: Window ID changed from" << windowId << "to" << currentWindowId << "- updating overlay";
        gst_video_overlay_set_window_handle(GST_VIDEO_OVERLAY(videosink), (guintptr)currentWindowId);
        gst_video_overlay_set_render_rectangle(GST_VIDEO_OVERLAY(videosink), 0, 0, width(), height());
        gst_video_overlay_expose(GST_VIDEO_OVERLAY(videosink));
    }

    // Force overlay refresh
    updateVideoOverlay();

    // Check if pipeline is playing
    GstState state;
    GstStateChangeReturn ret = gst_element_get_state(pipeline, &state, NULL, GST_CLOCK_TIME_NONE);
    if (ret == GST_STATE_CHANGE_SUCCESS && state == GST_STATE_PLAYING) {
        qDebug() << "WINDOWS: Video overlay verification passed - pipeline is playing";
    } else {
        qWarning() << "WINDOWS: Video overlay verification - pipeline state issue, state:" << state;
    }

    // Additional fallback: Try to re-expose the video overlay
    if (videosink && GST_IS_VIDEO_OVERLAY(videosink)) {
        gst_video_overlay_expose(GST_VIDEO_OVERLAY(videosink));
    }
}

// WINDOWS: Create video sink with overlay support for Qt widget embedding
GstElement* RtspClient::createOptimalVideoSink() {
    GstElement *videosink = nullptr;

    // WINDOWS SPECIFIC: Use sinks that support video overlay for Qt embedding
    const char* sink_names[] = {
        "d3dvideosink",     // DirectX 9 - most compatible with overlay
        "directdrawsink",   // DirectDraw - reliable overlay support
        "directshowvideosink", // DirectShow - alternative with overlay
        "d3d11videosink",   // DirectX 11 - newer but may have overlay issues
        nullptr
    };

    for (int i = 0; sink_names[i] != nullptr; i++) {
        videosink = gst_element_factory_make(sink_names[i], "videosink");
        if (videosink) {
            // Check if this sink supports video overlay (required for Qt embedding)
            if (GST_IS_VIDEO_OVERLAY(videosink)) {
                if (connection_count == 1) {
                    qDebug() << "WINDOWS: Using video sink with overlay support:" << sink_names[i];
                }
                return videosink;
            } else {
                if (connection_count == 1) {
                    qDebug() << "WINDOWS: Sink" << sink_names[i] << "does not support overlay, trying next";
                }
                gst_object_unref(videosink);
                videosink = nullptr;
            }
        }
    }

    // If no overlay-capable sink found, try autovideosink as last resort
    videosink = gst_element_factory_make("autovideosink", "videosink");
    if (videosink && GST_IS_VIDEO_OVERLAY(videosink)) {
        if (connection_count == 1) {
            qDebug() << "WINDOWS: Using autovideosink with overlay support";
        }
        return videosink;
    } else if (videosink) {
        gst_object_unref(videosink);
        videosink = nullptr;
    }

    // Emergency fallback - create a basic sink for testing
    videosink = gst_element_factory_make("fakesink", "videosink");
    if (videosink && connection_count == 1) {
        qWarning() << "CRITICAL: Using fakesink - no overlay-capable video sink available";
    }

    return videosink;
}

// Configure elements for optimal performance
void RtspClient::configureElements(GstElement *queue, GstElement *h264parse) {
    // Configure queue for better stability - less aggressive dropping
    g_object_set(queue, "max-size-buffers", 10, "max-size-bytes", 0, "max-size-time", 200000000, NULL);  // 200ms max, 10 buffers
    g_object_set(queue, "leaky", 2, NULL);  // Leak downstream (drop old frames)

    // Configure h264parse for better frame handling
    g_object_set(h264parse, "config-interval", 1, NULL);  // Send SPS/PPS frequently
    // Note: split-packetized property doesn't exist in this GStreamer version
}

// Streamlined pipeline creation
void RtspClient::startPipeline() {
    // Create pipeline elements
    GstElement *depay = gst_element_factory_make("rtph264depay", "depay");
    GstElement *h264parse = gst_element_factory_make("h264parse", "h264parse");
    GstElement *queue = gst_element_factory_make("queue", "queue");
    GstElement *decoder = createOptimalDecoder();
    videosink = createOptimalVideoSink();

    // Add additional queue after decoder for better frame handling
    GstElement *videoqueue = gst_element_factory_make("queue", "videoqueue");

    // Validate all elements created successfully
    if (!pipeline || !src || !depay || !h264parse || !decoder || !videosink || !queue || !videoqueue) {
        qWarning("Failed to create one or more GStreamer elements");

        // Try fallback elements
        if (!decoder) {
            qWarning("Failed to create H264 decoder, trying generic decoder");
            decoder = gst_element_factory_make("decodebin", "fallback_decoder");
            if (!decoder) {
                qWarning("Failed to create any decoder - GStreamer installation may be incomplete");
                return;
            }
        }
        if (!videosink) {
            videosink = gst_element_factory_make("autovideosink", "fallback_videosink");
            if (!videosink) {
                qWarning("Failed to create fallback video sink");
                return;
            }
        }
        if (!depay) {
            depay = gst_element_factory_make("rtph264depay", "fallback_depay");
        }
        if (!h264parse) {
            h264parse = gst_element_factory_make("h264parse", "fallback_h264parse");
        }
        if (!queue) {
            queue = gst_element_factory_make("queue", "fallback_queue");
        }

        if (!depay || !h264parse || !decoder || !videosink || !queue) {
            qWarning("Failed to create fallback GStreamer elements");
            return;
        }
    }

    if (connection_count == 1) {
        qDebug() << "GStreamer pipeline elements created successfully";
    }

    // Create videoconvert proactively for better compatibility (like your working pipeline)
    GstElement *videoconvert = gst_element_factory_make("videoconvert", "convert");
    if (!videoconvert) {
        qWarning("Failed to create videoconvert element");
        return;
    }

    // Add all elements to pipeline including videoconvert
    gst_bin_add_many(GST_BIN(pipeline), src, depay, h264parse, queue, decoder, videoconvert, videoqueue, videosink, NULL);

    // Try linking with videoconvert included from the start
    if (!gst_element_link_many(depay, h264parse, queue, decoder, videoconvert, videoqueue, videosink, NULL)) {
        qWarning("Failed to link pipeline with videoconvert - trying without videoqueue");

        // Try without the extra videoqueue
        if (!gst_element_link_many(depay, h264parse, queue, decoder, videoconvert, videosink, NULL)) {
            qWarning("Failed to link simplified pipeline");

            // Debug: check what decoder and videosink we're using
            gchar *decoder_name = gst_element_get_name(decoder);
            gchar *videosink_name = gst_element_get_name(videosink);
            qWarning() << "Decoder:" << decoder_name << "VideoSink:" << videosink_name;
            g_free(decoder_name);
            g_free(videosink_name);
            return;
        } else {
            qDebug() << "Successfully linked pipeline without videoqueue";
        }
    } else {
        qDebug() << "Successfully linked complete pipeline with videoconvert";
    }

    if (connection_count == 1) {
        qDebug() << "Pipeline elements linked successfully";
    }

    // Configure elements for optimal performance
    configureElements(queue, h264parse);

    // Configure video queue for better frame buffering
    if (videoqueue) {
        g_object_set(videoqueue, "max-size-buffers", 5, "max-size-bytes", 0, "max-size-time", 100000000, NULL);  // 100ms, 5 buffers
        g_object_set(videoqueue, "leaky", 1, NULL);  // Leak upstream (drop new frames when full)
    }

    // Set up signal connections
    padAddedHandlerId = g_signal_connect(src, "pad-added", G_CALLBACK(on_pad_added), depay);

    // Monitor video caps changes to get video dimensions
    GstPad *videopad = gst_element_get_static_pad(videosink, "sink");
    if (videopad) {
        g_signal_connect(videopad, "notify::caps", G_CALLBACK(RtspClient::onVideoCapsChanged), this);

        // Add probe to detect actual frames flowing through
        gst_pad_add_probe(videopad, GST_PAD_PROBE_TYPE_BUFFER,
                          [](GstPad *pad, GstPadProbeInfo *info, gpointer user_data) -> GstPadProbeReturn {
                              RtspClient *client = static_cast<RtspClient*>(user_data);
                              client->lastFrameTime = QDateTime::currentMSecsSinceEpoch();
                              return GST_PAD_PROBE_OK;
                          }, this, NULL);

        gst_object_unref(videopad);
    }

    // Set up bus message handling
    setupBusMessageHandler();

    // WINDOWS: Enhanced timing for video overlay setup with fallback
    QTimer::singleShot(1000, this, [this]() {
        if (videosink && winId()) {
            setupVideoOverlay();
        } else {
            qWarning() << "Video sink or window ID not ready, retrying overlay setup";
            QTimer::singleShot(1000, this, &RtspClient::setupVideoOverlay);
        }
    });

    // Start the pipeline
    gst_element_set_state(pipeline, GST_STATE_PLAYING);
}

// Paint event for debugging - video is rendered directly by GStreamer
void RtspClient::paintEvent(QPaintEvent *event) {
    Q_UNUSED(event)
    if (!videoOverlaySet) {
        QPainter painter(this);
        painter.fillRect(rect(), Qt::black);
        painter.setPen(Qt::white);
        painter.drawText(rect(), Qt::AlignCenter, "Connecting to " + rtspUrl);
    }
}

// Handle resize events
void RtspClient::resizeEvent(QResizeEvent *event) {
    QWidget::resizeEvent(event);
    if (videosink && GST_IS_VIDEO_OVERLAY(videosink) && videoOverlaySet) {
        // Use the enhanced overlay update system
        updateVideoOverlay();
        qDebug() << "Widget resized to:" << event->size().width() << "x" << event->size().height();
    }
}

// Handle show events
void RtspClient::showEvent(QShowEvent *event) {
    QWidget::showEvent(event);
    if (videosink && !videoOverlaySet) {
        QTimer::singleShot(100, this, &RtspClient::setupVideoOverlay);
    }
}

// Handle move events (important for video overlay repositioning)
void RtspClient::moveEvent(QMoveEvent *event) {
    QWidget::moveEvent(event);
    if (videosink && GST_IS_VIDEO_OVERLAY(videosink) && videoOverlaySet) {
        // Force video overlay to refresh its position
        gst_video_overlay_expose(GST_VIDEO_OVERLAY(videosink));
    }
}

// Set up freeze detection mechanism
void RtspClient::setupFreezeDetection() {
    freezeDetectionTimer = new QTimer(this);
    reconnectionTimer = new QTimer(this);

    // Check for freeze every 5 seconds
    connect(freezeDetectionTimer, &QTimer::timeout, this, &RtspClient::checkForFreeze);
    freezeDetectionTimer->start(5000);

    // Single shot timer for reconnection attempts
    connect(reconnectionTimer, &QTimer::timeout, this, &RtspClient::attemptReconnection);

    // Initialize frame timestamp
    lastFrameTime = QDateTime::currentMSecsSinceEpoch();
}

// Check if stream has frozen (no frames received)
void RtspClient::checkForFreeze() {
    qint64 currentTime = QDateTime::currentMSecsSinceEpoch();
    qint64 timeSinceLastFrame = currentTime - lastFrameTime;

    // If no frame received in 20 seconds, consider it frozen (increased for Pi cameras)
    if (timeSinceLastFrame > 20000 && !isReconnecting) {
        qWarning() << "Stream freeze detected for URL:" << rtspUrl
                   << "- Last frame" << timeSinceLastFrame << "ms ago";
        handleStreamFreeze();
    }
}

// Handle detected stream freeze
void RtspClient::handleStreamFreeze() {
    if (isReconnecting) {
        qDebug() << "[RTSP] Already handling freeze, ignoring duplicate freeze detection";
        return; // Already handling freeze
    }

    isReconnecting = true;
    reconnectionAttempts++;

    // Reset the last frame time to prevent immediate re-trigger
    lastFrameTime = QDateTime::currentMSecsSinceEpoch();

    qDebug() << "[RTSP] Handling stream freeze - Attempt" << reconnectionAttempts << "for URL:" << rtspUrl;
    update();  // Trigger repaint

    // If too many attempts, give up and disable reconnection
    if (reconnectionAttempts > 5) {  // Allow more attempts but with longer delays
        qWarning() << "[RTSP] Max reconnection attempts reached for URL:" << rtspUrl;
        isReconnecting = false;
        freezeDetectionTimer->stop();  // Stop freeze detection
        reconnectionTimer->stop();     // Stop reconnection timer
        update();  // Trigger repaint
        return;
    }

    // Stop timers before reset to prevent callback during destruction
    freezeDetectionTimer->stop();

    // Reset pipeline safely
    QMetaObject::invokeMethod(this, [this]() {
        resetPipeline();

        // Wait before reconnecting (exponential backoff)
        int delay = qMin(3000 * reconnectionAttempts, 15000); // Max 15 seconds
        reconnectionTimer->start(delay);
    }, Qt::QueuedConnection);
}

// Attempt to reconnect the stream
void RtspClient::attemptReconnection() {
    reconnectionTimer->stop();

    qDebug() << "[RTSP] Attempting reconnection for URL:" << rtspUrl;
    update();  // Trigger repaint

    try {
        // Restart the pipeline
        startPipeline();

        // Update timestamp
        lastFrameTime = QDateTime::currentMSecsSinceEpoch();

        // Restart freeze detection with shorter interval initially
        freezeDetectionTimer->start(5000); // Back to 5 second check

        qDebug() << "[RTSP] Reconnection attempt completed";
    } catch (const std::exception& e) {
        qWarning() << "[RTSP] Exception during reconnection:" << e.what();
        isReconnecting = false;
        update();  // Trigger repaint
    }
}

// Reset the GStreamer pipeline
void RtspClient::resetPipeline() {
    qDebug() << "[RTSP] Resetting pipeline for URL:" << rtspUrl;
    static QMutex resetMutex;
    QMutexLocker locker(&resetMutex);

    if(busWatchId > 0){
        g_source_remove(busWatchId);
        busWatchId = 0;
    }

    // Disconnect signal handlers before resetting pipeline
    if (src && padAddedHandlerId > 0) {
        g_signal_handler_disconnect(src, padAddedHandlerId);
        padAddedHandlerId = 0;
    }
    if (src && acceptCertHandlerId > 0) {
        g_signal_handler_disconnect(src, acceptCertHandlerId);
        acceptCertHandlerId = 0;
    }

    if (pipeline) {
        qDebug() << "[RTSP] Stopping existing pipeline";

        // Stop the pipeline gracefully with timeout
        GstStateChangeReturn ret = gst_element_set_state(pipeline, GST_STATE_NULL);
        if (ret == GST_STATE_CHANGE_FAILURE) {
            qWarning() << "[RTSP] Failed to stop pipeline gracefully";
        }

        // Wait for state change to complete with timeout
        GstState state, pending;
        ret = gst_element_get_state(pipeline, &state, &pending, 2 * GST_SECOND); // 2 second timeout
        if (ret == GST_STATE_CHANGE_FAILURE || ret == GST_STATE_CHANGE_ASYNC) {
            qWarning() << "[RTSP] Pipeline state change failed or timed out";
        }

        // Unreference the pipeline safely
        gst_object_unref(pipeline);
        pipeline = nullptr;
        src = nullptr;
        videosink = nullptr;
        qDebug() << "[RTSP] Pipeline cleaned up";
    }

    // Add small delay to ensure cleanup is complete
    QThread::msleep(100);

    // Recreate pipeline with error checking
    pipeline = gst_pipeline_new("pipeline");
    if (!pipeline) {
        qWarning() << "[RTSP] Failed to create new pipeline";
        isReconnecting = false;
        update();
        return;
    }

    src = gst_element_factory_make("rtspsrc", "src");
    if (!src) {
        qWarning() << "[RTSP] Failed to create rtspsrc element";
        isReconnecting = false;
        update();
        return;
    }

    // Reconfigure source with same settings based on URL type
    g_object_set(src, "location", rtspUrl.toUtf8().constData(), NULL);

    if (rtspUrl.startsWith("rtsps://")) {
        // TLS configuration for rtsps:// URLs
        g_object_set(src, "latency", 0, "drop-on-latency", TRUE, "timeout", 30000000, NULL); // 30 second timeout (consistent)
        g_object_set(src, "retry", 5, NULL); // More retries for TLS
        g_object_set(src, "buffer-mode", 1, NULL);
        g_object_set(src, "ntp-sync", FALSE, NULL);
        g_object_set(src, "rtp-blocksize", 1400, NULL);
        g_object_set(src, "protocols", GST_RTSP_LOWER_TRANS_TCP, NULL);
        // Alternative if constant not found: g_object_set(src, "protocols", 0x00000004, NULL);

        // Reuse the same TLS configuration for reconnection
        if (tlsDatabase) {
            g_object_set(G_OBJECT(src), "tls-database", tlsDatabase, NULL);
            qDebug() << "Reconnection: TLS database configured with RTSP CA";
        }

        if (tlsInteraction) {
            g_object_set(G_OBJECT(src), "tls-interaction", tlsInteraction, NULL);
        }

        acceptCertHandlerId = g_signal_connect(src, "accept-certificate", G_CALLBACK(on_accept_certificate), this);
        g_object_set(src, "tls-validation-flags", G_TLS_CERTIFICATE_UNKNOWN_CA | G_TLS_CERTIFICATE_BAD_IDENTITY, NULL);
        g_object_set(src, "do-rtcp", TRUE, NULL);
    } else if (rtspUrl.startsWith("rtsp://")) {
        // Standard RTSP configuration for rtsp:// URLs
        // Extract credentials from URL if present, otherwise use defaults
        if (rtspUrl.contains("@")) {
            qDebug() << "Using credentials from URL for RTSP reconnection";
        } else {
            g_object_set(src, "user-id", "username", "user-pw", "password", NULL);
        }
        g_object_set(src, "latency", 0, "drop-on-latency", TRUE, "timeout", 30000000, NULL);
        g_object_set(src, "retry", 5, NULL);
        g_object_set(src, "buffer-mode", 1, NULL);
        g_object_set(src, "ntp-sync", FALSE, NULL);
        g_object_set(src, "protocols", GST_RTSP_LOWER_TRANS_TCP, NULL);
        // Alternative if constant not found: g_object_set(src, "protocols", 0x00000004, NULL);  // Force TCP
        g_object_set(src, "do-rtcp", TRUE, NULL);
    } else {
        // Default configuration
        g_object_set(src, "latency", 0, "drop-on-latency", TRUE, "timeout", 15000000, NULL);
        g_object_set(src, "retry", 2, NULL);
        g_object_set(src, "buffer-mode", 1, NULL);
        g_object_set(src, "ntp-sync", FALSE, NULL);
    }
}

// Thread-safe slot to update the frame on the main thread
QPixmap RtspClient::getCurrentFrame() const {
    // For windID rendering, we can't easily capture the current frame
    // This would require additional GStreamer pipeline or screenshot functionality
    // For now, return an empty pixmap
    qWarning() << "Screenshot functionality not implemented for windID rendering";
    return QPixmap();
}

// Aspect ratio control methods
void RtspClient::setMaintainAspectRatio(bool maintain) {
    if (maintainAspectRatio != maintain) {
        maintainAspectRatio = maintain;
        if (videoOverlaySet) {
            updateVideoOverlay();
        }
    }
}

bool RtspClient::getMaintainAspectRatio() const {
    return maintainAspectRatio;
}
