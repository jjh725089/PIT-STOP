#ifndef RTSPCLIENT_H
#define RTSPCLIENT_H

#include <QWidget>
#include <QString>
// Undefine 'signals' macro from Qt to prevent conflict with GLib
#ifdef signals
#undef signals
#endif

#include <gst/gst.h>
#include <gst/video/videooverlay.h>
#include <gst/rtsp/gstrtspconnection.h>
#include <gst/rtsp/gstrtsptransport.h>
#include <QTimer>
#include <QtMqtt/QMqttClient>
#include <QSqlDatabase>
#include <QFile>
#include <QDateTime>
#include <QThread>
#include <QMetaObject>


#include <QTemporaryFile>
#include "rtspclienttlsinteraction.h"

class RtspClient : public QWidget
{
    Q_OBJECT

public:
    explicit RtspClient(const QString& url, QWidget *parent = nullptr);
    ~RtspClient();
    QString getTempCertPath() const;
    
    // Get current frame for snapshots (screenshot capability)
    QPixmap getCurrentFrame() const;
    
    // Aspect ratio control
    void setMaintainAspectRatio(bool maintain);
    bool getMaintainAspectRatio() const;
    
    // Camera control functions
    void startStream();
    void stopStream();
    bool isStreamActive() const;
    
    // TLS database access for certificate validation
    GTlsDatabase* getTlsDatabase() const { return tlsDatabase; }

protected:
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void showEvent(QShowEvent *event) override;
    void moveEvent(QMoveEvent *event) override;

private:
    QString rtspUrl;
    GstElement* pipeline;
    GstElement* src;
    GstElement* videosink;
    QTemporaryFile tempCertFile;
    GTlsDatabase *tlsDatabase;
    GTlsCertificate *ca_cert;
    RtspClientTlsInteraction *tlsInteraction;
    static int connection_count;
    guint busWatchId;
    gulong padAddedHandlerId;
    bool videoOverlaySet;
    
    // Video rendering properties
    int videoWidth;
    int videoHeight;
    bool maintainAspectRatio;
    QTimer *overlayUpdateTimer;
    gulong acceptCertHandlerId;
    
    // Freeze detection and recovery
    QTimer *freezeDetectionTimer;
    QTimer *reconnectionTimer;
    qint64 lastFrameTime;
    int reconnectionAttempts;
    bool isReconnecting;
    
    // Static helper functions
    static gboolean onBusMessage(GstBus *bus, GstMessage *msg, gpointer user_data);
    
    // Private helper functions
    void initializeClient();
    void checkGStreamerPlugins();
    void setupTlsConfiguration();
    void startPipeline();
    GstElement* createOptimalDecoder();
    GstElement* createOptimalVideoSink();
    void configureElements(GstElement *queue, GstElement *h264parse);
    void setupVideoOverlay();
    void updateVideoOverlay();
    void calculateOptimalRenderRect(int &x, int &y, int &width, int &height);
    void verifyVideoOverlay(WId windowId);
    static void onVideoCapsChanged(GstPad *pad, GParamSpec *pspec, gpointer user_data);
    void setupBusMessageHandler();
    
    // Freeze detection and recovery functions
    void setupFreezeDetection();
    void checkForFreeze();
    void handleStreamFreeze();
    void attemptReconnection();
    void resetPipeline();
};
#endif // RTSPCLIENT_H
