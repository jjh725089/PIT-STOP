#include "rtspclientcamera1.h"
#include <QTimer>
#include <QDebug>
#include <QMutex>
#include <QPainter>
#include <QPaintEvent>
#include <QResizeEvent>
#include <QDir>
#include <QApplication>
#include <QDateTime>
#include <QMutexLocker>
#include <QThread>

#ifdef G_OS_WIN32
#include <stdlib.h>
#endif

// Initialize static member
int RtspClientCamera1::connection_count = 0;

// Undefine 'signals' macro from Qt to prevent conflict with GLib
#ifdef signals
#undef signals
#endif

#include <gio/gio.h>

// Callback to handle TLS certificate acceptance (reused from original)
static gboolean on_accept_certificate_cam1(GstElement *src, GTlsConnection *conn, GTlsCertificate *peer_cert, GTlsCertificateFlags errors, gpointer user_data) {
    RtspClientCamera1 *client = static_cast<RtspClientCamera1*>(user_data);
    
    qDebug() << "[Camera1] Certificate validation - errors:" << errors;
    
    // Reject expired and revoked certificates
    if (errors & G_TLS_CERTIFICATE_EXPIRED) {
        qWarning() << "[Camera1] REJECTED: Certificate expired";
        return FALSE;
    }
    
    if (errors & G_TLS_CERTIFICATE_REVOKED) {
        qWarning() << "[Camera1] REJECTED: Certificate revoked";
        return FALSE;
    }
    
    // Accept certificates with UNKNOWN_CA (self-signed) and BAD_IDENTITY
    if ((errors & G_TLS_CERTIFICATE_UNKNOWN_CA) || (errors & G_TLS_CERTIFICATE_BAD_IDENTITY)) {
        qDebug() << "[Camera1] ACCEPTED: Certificate with UNKNOWN_CA or BAD_IDENTITY (expected for self-signed)";
        return TRUE;
    }
    
    // Accept certificates with no validation errors
    if (errors == G_TLS_CERTIFICATE_NO_FLAGS) {
        qDebug() << "[Camera1] ACCEPTED: Certificate has no validation errors";
        return TRUE;
    }
    
    qDebug() << "[Camera1] ACCEPTED: Certificate with minor errors:" << errors;
    return TRUE;
}

// Callback to handle dynamic pad creation by rtspsrc
static void on_pad_added_cam1(GstElement *element, GstPad *pad, gpointer user_data) {
    GstCaps *caps = gst_pad_get_current_caps(pad);
    if (caps) {
        const gchar *name = gst_structure_get_name(gst_caps_get_structure(caps, 0));
        if (g_str_has_prefix(name, "application/x-rtp")) {
            GstElement *depay = (GstElement *)user_data;
            GstPad *sinkpad = gst_element_get_static_pad(depay, "sink");

            if (gst_pad_is_linked(sinkpad)) {
                qDebug("[Camera1] Sink pad already linked - skipping");
                gst_object_unref(sinkpad);
                gst_caps_unref(caps);
                return;
            }

            GstPadLinkReturn result = gst_pad_link(pad, sinkpad);
            if (result != GST_PAD_LINK_OK) {
                qWarning("[Camera1] Failed to link dynamic pad to depayloader: %d", result);
            } else {
                qDebug("[Camera1] Linked dynamic RTP pad to depayloader");
            }
            gst_object_unref(sinkpad);
        }
        gst_caps_unref(caps);
    }
}

RtspClientCamera1::RtspClientCamera1(const QString& url, QWidget *parent)
    : QLabel(parent)
    , rtspUrl(url)
    , pipeline(nullptr)
    , src(nullptr)
    , appsink(nullptr)
    , tlsDatabase(nullptr)
    , ca_cert(nullptr)
    , tlsInteraction(nullptr)
    , frameTimer(nullptr)
    , freezeDetectionTimer(nullptr)
    , reconnectionTimer(nullptr)
    , lastFrameTime(0)
    , reconnectionAttempts(0)
    , isReconnecting(false)
    , busWatchId(0)
    , padAddedHandlerId(0)
    , acceptCertHandlerId(0)
    , isInitialized(false)
    , maintainAspectRatio(false)
    , distortionEnabled(false)
    , k1(0), k2(0), k3(0), p1(0), p2(0)
    , fx(1), fy(1), cx(0), cy(0)
    , mapsInitialized(false)
{
    qDebug() << "[Camera1] Creating specialized RTSP client with OpenCV processing for URL:" << rtspUrl;
    
    // Set up QLabel properties
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    setMinimumSize(500, 300);
    setAlignment(Qt::AlignCenter);
    setStyleSheet("border: 2px solid #333; background-color: black; color: white;");
    setText("Camera 1: Connecting with OpenCV processing...");
    
    // Setup OpenCL if available
    setupOpenCL();
    
    // Initialize frame processing timer
    frameTimer = new QTimer(this);
    connect(frameTimer, &QTimer::timeout, this, &RtspClientCamera1::processFrame);
    //frameTimer->start(50); // ~20 FPS processing (lower CPU usage)
    frameTimer->start(40); // ~25 FPS processing (higher CPU usage)
    //frameTimer->start(33); // ~30 FPS processing (higher CPU usage)
    
    connection_count++;
    
    // Staggered initialization
    QTimer::singleShot(100, this, [this]() {
        initializeClient();
    });
}

RtspClientCamera1::~RtspClientCamera1()
{
    // Clean up timers
    if (frameTimer) {
        frameTimer->stop();
        delete frameTimer;
    }
    if (freezeDetectionTimer) {
        freezeDetectionTimer->stop();
        delete freezeDetectionTimer;
    }
    if (reconnectionTimer) {
        reconnectionTimer->stop();
        delete reconnectionTimer;
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

void RtspClientCamera1::setupOpenCL()
{
    // Initialize OpenCL if available
    if (cv::ocl::haveOpenCL()) {
        cv::ocl::setUseOpenCL(true);
        qDebug() << "[Camera1] OpenCL enabled for GPU acceleration";
        
        // Get OpenCL platform info
        cv::ocl::Context ctx = cv::ocl::Context::fromDevice(cv::ocl::Device::getDefault());
        if (!ctx.empty()) {
            cv::String deviceName = ctx.device(0).name();
            qDebug() << "[Camera1] OpenCL device:" << QString::fromStdString(deviceName);
        }
    } else {
        qWarning() << "[Camera1] OpenCL not available - using CPU processing";
    }
}


void RtspClientCamera1::checkGStreamerPlugins() {
    qDebug() << "[Camera1] Checking GStreamer plugin availability...";

    const char* essential_plugins[] = {
        "rtspsrc", "rtph264depay", "h264parse", "queue",
        "avdec_h264", "d3d11h264dec", "appsink", "videoconvert",
        nullptr
    };

    for (int i = 0; essential_plugins[i] != nullptr; i++) {
        GstElementFactory *factory = gst_element_factory_find(essential_plugins[i]);
        if (factory) {
            qDebug() << "[Camera1] ✓" << essential_plugins[i] << "available";
            gst_object_unref(factory);
        } else {
            qWarning() << "[Camera1] ✗" << essential_plugins[i] << "NOT available";
        }
    }
}

void RtspClientCamera1::initializeClient() {
    // Set minimal debugging
    gst_debug_set_default_threshold(GST_LEVEL_ERROR);
    gst_debug_set_threshold_for_name("rtspsrc", GST_LEVEL_ERROR);
    gst_debug_set_threshold_for_name("rtspconnection", GST_LEVEL_ERROR);
    gst_debug_set_threshold_for_name("videodecoder", GST_LEVEL_ERROR);

    checkGStreamerPlugins();

    if (rtspUrl.isEmpty()) {
        qWarning() << "[Camera1] Empty RTSP URL provided";
        return;
    }

    qDebug() << "[Camera1] Initializing specialized client for URL:" << rtspUrl;

    // Disable TLS debugging environment variable
#ifdef G_OS_WIN32
    _putenv_s("G_TLS_DEBUG", "");
#else
    g_unsetenv("G_TLS_DEBUG");
#endif

    // Update display
    setText("Camera 1: Initializing pipeline...");

    pipeline = gst_pipeline_new("camera1-pipeline");
    src = gst_element_factory_make("rtspsrc", "src");

    g_object_set(src, "location", rtspUrl.toUtf8().constData(), NULL);

    // Configure RTSP source for low latency
    g_object_set(src, "latency", 0, "drop-on-latency", TRUE, "timeout", 30000000, NULL);
    g_object_set(src, "retry", 5, NULL);
    g_object_set(src, "buffer-mode", 1, NULL);
    g_object_set(src, "ntp-sync", FALSE, NULL);
    g_object_set(src, "rtp-blocksize", 1400, NULL);

    // Set credentials
    g_object_set(src, "user-id", "admin", "user-pw", "veda1357!", NULL);

    // Configure protocol based on URL type
    if (rtspUrl.startsWith("rtsps://")) {
        g_object_set(src, "protocols", GST_RTSP_LOWER_TRANS_TCP, NULL);
        setupTlsConfiguration();
    } else if (rtspUrl.startsWith("rtsp://")) {
        g_object_set(src, "protocols", GST_RTSP_LOWER_TRANS_TCP, NULL);
    }

    startPipeline();
    setupFreezeDetection();
    
    isInitialized = true;
}

void RtspClientCamera1::setupTlsConfiguration() {
    qDebug() << "[Camera1] Setting up TLS configuration for RTSPS connection";
    
    // Extract the CA certificate from resources to a temporary file
    QFile certResourceFile(":/cert/ca/certs/ca.cert.pem");
    if (certResourceFile.open(QIODevice::ReadOnly)) {
        QByteArray certData = certResourceFile.readAll();
        qDebug() << "[Camera1] CA certificate data size:" << certData.size() << "bytes";
        
        if (tempCertFile.open()) {
            tempCertFile.write(certData);
            tempCertFile.close();
            qDebug() << "[Camera1] Certificate written to temporary file:" << tempCertFile.fileName();
        } else {
            qWarning() << "[Camera1] Failed to create temporary certificate file";
            return;
        }
        certResourceFile.close();
    } else {
        qWarning() << "[Camera1] Failed to open CA certificate resource file";
        return;
    }

    // Load the CA certificate into a GTlsDatabase
    GError *error = NULL;
    QByteArray certPathBytes = tempCertFile.fileName().toUtf8();
    const gchar *cert_path = certPathBytes.constData();
    
    qDebug() << "[Camera1] Using certificate path:" << cert_path;

    tlsDatabase = g_tls_file_database_new(cert_path, &error);
    if (error) {
        qWarning("[Camera1] Failed to create TLS database: %s", error->message);
        g_error_free(error);
        tlsDatabase = nullptr;
    } else {
        qDebug() << "[Camera1] TLS database created successfully";
    }

    ca_cert = g_tls_certificate_new_from_file(cert_path, &error);
    if(!ca_cert){
        qWarning() << "[Camera1] Failed to parse CA PEM:" << error->message;
        g_error_free(error);
        return;
    } else {
        qDebug() << "[Camera1] CA certificate loaded successfully";
    }
    
    // Enable TLS database for proper certificate validation
    if (tlsDatabase) {
        g_object_set(G_OBJECT(src), "tls-database", tlsDatabase, NULL);
        qDebug() << "[Camera1] TLS database configured with RTSP CA";
    }
    
    // Create and enable the custom TLS interaction object
    tlsInteraction = rtsp_client_tls_interaction_new(nullptr, ca_cert, tlsDatabase);
    if (tlsInteraction) {
        g_object_set(G_OBJECT(src), "tls-interaction", tlsInteraction, NULL);
        qDebug() << "[Camera1] TLS interaction object set on rtspsrc";
    }
    
    // Add direct signal handler for TLS errors
    acceptCertHandlerId = g_signal_connect(src, "accept-certificate", G_CALLBACK(on_accept_certificate_cam1), this);
    
    // Configure TLS validation
    g_object_set(src, "tls-validation-flags", G_TLS_CERTIFICATE_UNKNOWN_CA | G_TLS_CERTIFICATE_BAD_IDENTITY, NULL);
    g_object_set(src, "do-rtcp", TRUE, NULL);
    
    qDebug() << "[Camera1] TLS configuration completed";
}

GstElement* RtspClientCamera1::createOptimalDecoder() {
    GstElement *decoder = nullptr;

    // Check available decoders in order of preference for hardware acceleration
    const char* decoder_names[] = {
        "d3d11h264dec",     // D3D11 hardware decoder (preferred for Windows)
        "mfh264dec",        // Media Foundation decoder
        "avdec_h264",       // FFmpeg software decoder
        "openh264dec",      // OpenH264 decoder
        nullptr
    };

    for (int i = 0; decoder_names[i] != nullptr; i++) {
        decoder = gst_element_factory_make(decoder_names[i], "decoder");
        if (decoder) {
            qDebug() << "[Camera1] Using decoder:" << decoder_names[i];
            return decoder;
        }
    }

    // Fallback to generic decoder
    decoder = gst_element_factory_make("decodebin", "decoder");
    if (decoder) {
        qDebug() << "[Camera1] Using generic decodebin decoder";
    }

    return decoder;
}

void RtspClientCamera1::configureElements(GstElement *queue, GstElement *h264parse) {
    // Configure queue for better performance with appsink
    g_object_set(queue, "max-size-buffers", 5, "max-size-bytes", 0, "max-size-time", 100000000, NULL);  // 100ms, 5 buffers
    g_object_set(queue, "leaky", 2, NULL);  // Leak downstream (drop old frames)

    // Configure h264parse
    g_object_set(h264parse, "config-interval", 1, NULL);  // Send SPS/PPS frequently
}

void RtspClientCamera1::startPipeline() {
    setText("Camera 1: Building pipeline...");
    
    // Create appsink-based pipeline elements
    GstElement *depay = gst_element_factory_make("rtph264depay", "depay");
    GstElement *h264parse = gst_element_factory_make("h264parse", "h264parse"); 
    GstElement *queue = gst_element_factory_make("queue", "queue");
    GstElement *decoder = createOptimalDecoder();
    GstElement *videoconvert = gst_element_factory_make("videoconvert", "convert");
    appsink = gst_element_factory_make("appsink", "appsink");

    // Validate all elements created successfully
    if (!pipeline || !src || !depay || !h264parse || !decoder || !videoconvert || !appsink || !queue) {
        qWarning("[Camera1] Failed to create one or more GStreamer elements");
        setText("Camera 1: Pipeline creation failed");
        return;
    }

    qDebug() << "[Camera1] All pipeline elements created successfully";

    // Configure appsink for OpenCV integration
    g_object_set(appsink, "emit-signals", TRUE, NULL);
    g_object_set(appsink, "max-buffers", 1, NULL);  // Keep only latest frame
    g_object_set(appsink, "drop", TRUE, NULL);      // Drop old frames
    g_object_set(appsink, "sync", FALSE, NULL);     // Don't sync to clock for lower latency
    
    // Set caps for appsink - request RGB format for easier OpenCV processing
    GstCaps *caps = gst_caps_new_simple("video/x-raw",
                                        "format", G_TYPE_STRING, "RGB",
                                        NULL);
    g_object_set(appsink, "caps", caps, NULL);
    gst_caps_unref(caps);

    // Connect new-sample signal
    g_signal_connect(appsink, "new-sample", G_CALLBACK(RtspClientCamera1::onNewSample), this);

    // Add all elements to pipeline
    gst_bin_add_many(GST_BIN(pipeline), src, depay, h264parse, queue, decoder, videoconvert, appsink, NULL);

    // Link static elements
    if (!gst_element_link_many(depay, h264parse, queue, decoder, videoconvert, appsink, NULL)) {
        qWarning("[Camera1] Failed to link pipeline elements");
        setText("Camera 1: Pipeline linking failed");
        return;
    }

    qDebug() << "[Camera1] Pipeline elements linked successfully";

    // Configure elements for optimal performance
    configureElements(queue, h264parse);

    // Set up signal connections
    padAddedHandlerId = g_signal_connect(src, "pad-added", G_CALLBACK(on_pad_added_cam1), depay);

    // Set up bus message handling
    setupBusMessageHandler();

    // Start the pipeline
    setText("Camera 1: Starting stream...");
    gst_element_set_state(pipeline, GST_STATE_PLAYING);
}

// Static callback for new samples from appsink
GstFlowReturn RtspClientCamera1::onNewSample(GstAppSink *appsink, gpointer user_data) {
    RtspClientCamera1 *client = static_cast<RtspClientCamera1*>(user_data);
    
    GstSample *sample = gst_app_sink_pull_sample(appsink);
    if (!sample) return GST_FLOW_ERROR;

    GstBuffer *buffer = gst_sample_get_buffer(sample);
    GstCaps *caps = gst_sample_get_caps(sample);
    
    if (!buffer || !caps) {
        gst_sample_unref(sample);
        return GST_FLOW_ERROR;
    }

    // Get video info from caps
    GstVideoInfo video_info;
    if (!gst_video_info_from_caps(&video_info, caps)) {
        gst_sample_unref(sample);
        return GST_FLOW_ERROR;
    }

    // Map buffer to read data
    GstMapInfo map;
    if (!gst_buffer_map(buffer, &map, GST_MAP_READ)) {
        gst_sample_unref(sample);
        return GST_FLOW_ERROR;
    }

    // Create OpenCV Mat from buffer data
    int width = GST_VIDEO_INFO_WIDTH(&video_info);
    int height = GST_VIDEO_INFO_HEIGHT(&video_info);
    
    // Create cv::Mat from RGB data
    cv::Mat frame(height, width, CV_8UC3, map.data);
    
    // Update frame with thread safety
    {
        QMutexLocker locker(&client->frameMutex);
        frame.copyTo(client->currentFrame);
        client->lastFrameTime = QDateTime::currentMSecsSinceEpoch();
    }

    // Clean up
    gst_buffer_unmap(buffer, &map);
    gst_sample_unref(sample);

    return GST_FLOW_OK;
}

void RtspClientCamera1::processFrame() {
    QMutexLocker locker(&frameMutex);
    
    if (currentFrame.empty()) {
        return;
    }

    // Process frame with OpenCV (with OpenCL acceleration if available)
    cv::Mat processedFrame = processOpenCVFrame(currentFrame);
    
    // Convert to QImage and update display
    QImage qimage = matToQImage(processedFrame);
    if (!qimage.isNull()) {
        updateDisplayFrame(qimage);
    }
}

cv::Mat RtspClientCamera1::processOpenCVFrame(const cv::Mat& inputFrame) {
    cv::Mat outputFrame;
    
    if (distortionEnabled && fx > 0 && fy > 0) {
        try {
            // Initialize undistortion maps once
            if (!mapsInitialized) {
                cv::Mat cameraMatrix = (cv::Mat_<double>(3,3) << 
                    fx, 0, cx,
                    0, fy, cy,
                    0, 0, 1);
                
                cv::Mat distCoeffs = (cv::Mat_<double>(1,5) << k1, k2, p1, p2, k3);
                
                cv::Mat tempMap1, tempMap2;
                cv::initUndistortRectifyMap(cameraMatrix, distCoeffs, cv::Mat(), 
                                          cameraMatrix, inputFrame.size(), CV_16SC2, tempMap1, tempMap2);
                
                // Upload to GPU once
                tempMap1.copyTo(map1);
                tempMap2.copyTo(map2);
                mapsInitialized = true;
                qDebug() << "[Camera1] GPU distortion maps initialized for size:" << inputFrame.size().width << "x" << inputFrame.size().height;
            }
            
            // Fast GPU remap with nearest neighbor
            cv::UMat inputUMat, outputUMat;
            inputFrame.copyTo(inputUMat);
            cv::remap(inputUMat, outputUMat, map1, map2, cv::INTER_NEAREST);
            outputUMat.copyTo(outputFrame);
            
        } catch (const cv::Exception& e) {
            qWarning() << "[Camera1] OpenCV exception in distortion correction:" << e.what();
            inputFrame.copyTo(outputFrame);
        }
    } else if (cv::ocl::useOpenCL()) {
        // Use OpenCL UMat for other GPU processing
        cv::UMat umat;
        inputFrame.copyTo(umat);
        umat.copyTo(outputFrame);
        qDebug() << "[Camera1] Using OpenCL without distortion correction";
    } else {
        // CPU processing fallback
        inputFrame.copyTo(outputFrame);
        qDebug() << "[Camera1] Using CPU processing fallback";
    }
    
    return outputFrame;
}

QImage RtspClientCamera1::matToQImage(const cv::Mat& mat) const {
    switch (mat.type()) {
        case CV_8UC3: {
            // RGB format
            return QImage(mat.data, mat.cols, mat.rows, mat.step, QImage::Format_RGB888);
        }
        case CV_8UC4: {
            // RGBA format
            return QImage(mat.data, mat.cols, mat.rows, mat.step, QImage::Format_RGBA8888);
        }
        case CV_8UC1: {
            // Grayscale
            return QImage(mat.data, mat.cols, mat.rows, mat.step, QImage::Format_Grayscale8);
        }
        default:
            qWarning() << "[Camera1] Unsupported Mat format for QImage conversion";
            return QImage();
    }
}

void RtspClientCamera1::updateDisplayFrame(const QImage& qimage) {
    if (qimage.isNull()) return;
    
    // Scale image to fit widget while maintaining aspect ratio if needed
    QSize widgetSize = size();
    QPixmap pixmap = QPixmap::fromImage(qimage);
    
    if (maintainAspectRatio) {
        pixmap = pixmap.scaled(widgetSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    } else {
        pixmap = pixmap.scaled(widgetSize, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
    }
    
    setPixmap(pixmap);
}

void RtspClientCamera1::setupBusMessageHandler() {
    GstBus *bus = gst_pipeline_get_bus(GST_PIPELINE(pipeline));
    busWatchId = gst_bus_add_watch(bus, onBusMessage, this);
    gst_object_unref(bus);
}

// Static function to handle bus messages
gboolean RtspClientCamera1::onBusMessage(GstBus *bus, GstMessage *msg, gpointer user_data) {
    RtspClientCamera1 *client = static_cast<RtspClientCamera1*>(user_data);

    switch (GST_MESSAGE_TYPE(msg)) {
        case GST_MESSAGE_ERROR: {
            GError *error;
            gchar *debug;
            gst_message_parse_error(msg, &error, &debug);
            qWarning() << "[Camera1 Error]" << client->rtspUrl << ":" << error->message;
            qWarning() << "[Camera1] Error domain:" << error->domain << "Code:" << error->code;
            qWarning() << "[Camera1] Debug info:" << (debug ? debug : "none");

            client->setText("Camera 1: Connection Error");

            g_error_free(error);
            g_free(debug);
            break;
        }
        case GST_MESSAGE_WARNING: {
            GError *error;
            gchar *debug;
            gst_message_parse_warning(msg, &error, &debug);
            qWarning() << "[Camera1] Pipeline warning:" << error->message;
            g_error_free(error);
            g_free(debug);
            break;
        }
        case GST_MESSAGE_STATE_CHANGED: {
            if (GST_MESSAGE_SRC(msg) == GST_OBJECT(client->src)) {
                GstState old_state, new_state;
                gst_message_parse_state_changed(msg, &old_state, &new_state, NULL);
                qDebug() << "[Camera1] RTSP source state changed from" << old_state << "to" << new_state;

                if (new_state == GST_STATE_PLAYING) {
                    qDebug() << "[Camera1] RTSP source is now playing with OpenCV processing";
                    client->reconnectionAttempts = 0;
                    client->setText("Camera 1: Processing frames with OpenCV...");
                    client->lastFrameTime = QDateTime::currentMSecsSinceEpoch();
                }
            }
            break;
        }
        default:
            break;
    }
    return TRUE;
}

// Camera control functions
void RtspClientCamera1::startStream() {
    if (!pipeline) {
        qDebug() << "[Camera1] Starting stream for URL:" << rtspUrl;
        initializeClient();
        return;
    }
    
    GstState currentState;
    gst_element_get_state(pipeline, &currentState, NULL, GST_CLOCK_TIME_NONE);
    
    if (currentState != GST_STATE_PLAYING) {
        qDebug() << "[Camera1] Starting stream playback for URL:" << rtspUrl;
        GstStateChangeReturn ret = gst_element_set_state(pipeline, GST_STATE_PLAYING);
        if (ret == GST_STATE_CHANGE_FAILURE) {
            qWarning() << "[Camera1] Failed to start stream for URL:" << rtspUrl;
            setText("Camera 1: Failed to start stream");
        }
    } else {
        qDebug() << "[Camera1] Stream already playing for URL:" << rtspUrl;
    }
}

void RtspClientCamera1::stopStream() {
    if (!pipeline) {
        qDebug() << "[Camera1] No pipeline to stop for URL:" << rtspUrl;
        return;
    }
    
    qDebug() << "[Camera1] Stopping stream for URL:" << rtspUrl;
    GstStateChangeReturn ret = gst_element_set_state(pipeline, GST_STATE_NULL);
    if (ret == GST_STATE_CHANGE_FAILURE) {
        qWarning() << "[Camera1] Failed to stop stream for URL:" << rtspUrl;
    }
    
    setText("Camera 1: Stream stopped");
}

bool RtspClientCamera1::isStreamActive() const {
    if (!pipeline) {
        return false;
    }
    
    GstState currentState;
    GstStateChangeReturn ret = gst_element_get_state(pipeline, &currentState, NULL, GST_CLOCK_TIME_NONE);
    
    return (ret != GST_STATE_CHANGE_FAILURE && currentState == GST_STATE_PLAYING);
}

QPixmap RtspClientCamera1::getCurrentFrame() const {
    QMutexLocker locker(&frameMutex);
    
    if (currentFrame.empty()) {
        qWarning() << "[Camera1] No current frame available for snapshot";
        return QPixmap();
    }
    
    QImage qimage = matToQImage(currentFrame);
    return QPixmap::fromImage(qimage);
}

void RtspClientCamera1::setMaintainAspectRatio(bool maintain) {
    maintainAspectRatio = maintain;
}

bool RtspClientCamera1::getMaintainAspectRatio() const {
    return maintainAspectRatio;
}

void RtspClientCamera1::resizeEvent(QResizeEvent *event) {
    QLabel::resizeEvent(event);
    // The next processFrame() call will handle the new size
}

// Freeze detection and recovery functions (simplified for appsink)
void RtspClientCamera1::setupFreezeDetection() {
    freezeDetectionTimer = new QTimer(this);
    reconnectionTimer = new QTimer(this);

    connect(freezeDetectionTimer, &QTimer::timeout, this, &RtspClientCamera1::checkForFreeze);
    freezeDetectionTimer->start(5000); // Check every 5 seconds

    connect(reconnectionTimer, &QTimer::timeout, this, &RtspClientCamera1::attemptReconnection);

    lastFrameTime = QDateTime::currentMSecsSinceEpoch();
}

void RtspClientCamera1::checkForFreeze() {
    qint64 currentTime = QDateTime::currentMSecsSinceEpoch();
    qint64 timeSinceLastFrame = currentTime - lastFrameTime;

    if (timeSinceLastFrame > 20000 && !isReconnecting) { // 20 seconds
        qWarning() << "[Camera1] Stream freeze detected - Last frame" << timeSinceLastFrame << "ms ago";
        handleStreamFreeze();
    }
}

void RtspClientCamera1::handleStreamFreeze() {
    if (isReconnecting) return;
    
    isReconnecting = true;
    reconnectionAttempts++;
    
    lastFrameTime = QDateTime::currentMSecsSinceEpoch();
    
    qDebug() << "[Camera1] Handling stream freeze - Attempt" << reconnectionAttempts;
    setText("Camera 1: Reconnecting...");
    
    if (reconnectionAttempts > 5) {
        qWarning() << "[Camera1] Max reconnection attempts reached";
        isReconnecting = false;
        freezeDetectionTimer->stop();
        reconnectionTimer->stop();
        setText("Camera 1: Connection failed");
        return;
    }
    
    freezeDetectionTimer->stop();
    
    QMetaObject::invokeMethod(this, [this]() {
        resetPipeline();
        int delay = qMin(3000 * reconnectionAttempts, 15000);
        reconnectionTimer->start(delay);
    }, Qt::QueuedConnection);
}

void RtspClientCamera1::attemptReconnection() {
    reconnectionTimer->stop();
    
    qDebug() << "[Camera1] Attempting reconnection";
    setText("Camera 1: Reconnecting...");
    
    try {
        startPipeline();
        lastFrameTime = QDateTime::currentMSecsSinceEpoch();
        freezeDetectionTimer->start(5000);
        qDebug() << "[Camera1] Reconnection attempt completed";
    } catch (const std::exception& e) {
        qWarning() << "[Camera1] Exception during reconnection:" << e.what();
        isReconnecting = false;
        setText("Camera 1: Reconnection failed");
    }
}

void RtspClientCamera1::setDistortionParameters(float k1_, float k2_, float k3_, float p1_, float p2_, float fx_, float fy_, float cx_, float cy_) {
    k1 = k1_; k2 = k2_; k3 = k3_; p1 = p1_; p2 = p2_;
    fx = fx_; fy = fy_; cx = cx_; cy = cy_;
    mapsInitialized = false;  // Mark maps as needing regeneration
    qDebug() << "[Camera1] Distortion parameters updated";
}

void RtspClientCamera1::enableDistortionCorrection(bool enable) {
    distortionEnabled = enable;
    qDebug() << "[Camera1] Distortion correction" << (distortionEnabled ? "enabled" : "disabled");
}

void RtspClientCamera1::resetPipeline() {
    qDebug() << "[Camera1] Resetting pipeline";
    
    if(busWatchId > 0){
        g_source_remove(busWatchId);
        busWatchId = 0;
    }

    if (src && padAddedHandlerId > 0) {
        g_signal_handler_disconnect(src, padAddedHandlerId);
        padAddedHandlerId = 0;
    }
    if (src && acceptCertHandlerId > 0) {
        g_signal_handler_disconnect(src, acceptCertHandlerId);
        acceptCertHandlerId = 0;
    }

    if (pipeline) {
        gst_element_set_state(pipeline, GST_STATE_NULL);
        
        GstState state, pending;
        GstStateChangeReturn ret = gst_element_get_state(pipeline, &state, &pending, 2 * GST_SECOND);
        if (ret == GST_STATE_CHANGE_FAILURE || ret == GST_STATE_CHANGE_ASYNC) {
            qWarning() << "[Camera1] Pipeline state change failed or timed out";
        }

        gst_object_unref(pipeline);
        pipeline = nullptr;
        src = nullptr;
        appsink = nullptr;
    }

    QThread::msleep(100);

    // Recreate pipeline
    pipeline = gst_pipeline_new("camera1-pipeline");
    if (!pipeline) {
        qWarning() << "[Camera1] Failed to create new pipeline";
        isReconnecting = false;
        setText("Camera 1: Pipeline creation failed");
        return;
    }

    src = gst_element_factory_make("rtspsrc", "src");
    if (!src) {
        qWarning() << "[Camera1] Failed to create rtspsrc element";
        isReconnecting = false;
        setText("Camera 1: Source creation failed");
        return;
    }

    // Reconfigure source with same settings
    g_object_set(src, "location", rtspUrl.toUtf8().constData(), NULL);
    g_object_set(src, "latency", 0, "drop-on-latency", TRUE, "timeout", 30000000, NULL);
    g_object_set(src, "retry", 5, NULL);
    g_object_set(src, "buffer-mode", 1, NULL);
    g_object_set(src, "ntp-sync", FALSE, NULL);
    g_object_set(src, "rtp-blocksize", 1400, NULL);
    
    // Set credentials
    g_object_set(src, "user-id", "admin", "user-pw", "veda1357!", NULL);

    if (rtspUrl.startsWith("rtsps://")) {
        g_object_set(src, "protocols", GST_RTSP_LOWER_TRANS_TCP, NULL);
        
        if (tlsDatabase) {
            g_object_set(G_OBJECT(src), "tls-database", tlsDatabase, NULL);
        }
        if (tlsInteraction) {
            g_object_set(G_OBJECT(src), "tls-interaction", tlsInteraction, NULL);
        }
        
        acceptCertHandlerId = g_signal_connect(src, "accept-certificate", G_CALLBACK(on_accept_certificate_cam1), this);
        g_object_set(src, "tls-validation-flags", G_TLS_CERTIFICATE_UNKNOWN_CA | G_TLS_CERTIFICATE_BAD_IDENTITY, NULL);
        g_object_set(src, "do-rtcp", TRUE, NULL);
    } else if (rtspUrl.startsWith("rtsp://")) {
        g_object_set(src, "protocols", GST_RTSP_LOWER_TRANS_TCP, NULL);
        g_object_set(src, "do-rtcp", TRUE, NULL);
    }
}
