#ifndef RTSPCLIENTCAMERA1_H
#define RTSPCLIENTCAMERA1_H

#include <QWidget>
#include <QString>
#include <QTimer>
#include <QLabel>
#include <QPixmap>
#include <QTemporaryFile>
#include <QMutex>
#include <QThread>

// Undefine 'signals' macro from Qt to prevent conflict with GLib
#ifdef signals
#undef signals
#endif

#include <gst/gst.h>
#include <gst/app/gstappsink.h>
#include <gst/video/videooverlay.h>
#include <gst/rtsp/gstrtspconnection.h>
#include <gst/rtsp/gstrtsptransport.h>

// OpenCV includes
#include <opencv2/opencv.hpp>
#include <opencv2/core/ocl.hpp>

#include "rtspclienttlsinteraction.h"

class RtspClientCamera1 : public QLabel
{
    Q_OBJECT

public:
    explicit RtspClientCamera1(const QString& url, QWidget *parent = nullptr);
    ~RtspClientCamera1();
    
    // Camera control functions
    void startStream();
    void stopStream();
    bool isStreamActive() const;
    
    // Get current frame for snapshots
    QPixmap getCurrentFrame() const;
    
    // Aspect ratio control
    void setMaintainAspectRatio(bool maintain);
    bool getMaintainAspectRatio() const;
    
    // Lens distortion correction control
    void setDistortionParameters(float k1, float k2, float k3, float p1, float p2, float fx, float fy, float cx, float cy);
    void enableDistortionCorrection(bool enable);

protected:
    void resizeEvent(QResizeEvent *event) override;

private slots:
    void processFrame();

private:
    // GStreamer pipeline components
    QString rtspUrl;
    GstElement* pipeline;
    GstElement* src;
    GstElement* appsink;
    QTemporaryFile tempCertFile;
    GTlsDatabase *tlsDatabase;
    GTlsCertificate *ca_cert;
    RtspClientTlsInteraction *tlsInteraction;
    
    // OpenCV processing
    cv::Mat currentFrame;
    cv::UMat umatFrame;  // OpenCL UMat for GPU processing
    mutable QMutex frameMutex;
    
    // Lens distortion correction
    bool distortionEnabled;
    float k1, k2, k3, p1, p2;  // Distortion coefficients
    float fx, fy, cx, cy;      // Camera intrinsics
    cv::UMat map1, map2;       // Precomputed undistortion maps (GPU)
    bool mapsInitialized;
    QTimer *frameTimer;
    
    // State tracking
    static int connection_count;
    guint busWatchId;
    gulong padAddedHandlerId;
    gulong acceptCertHandlerId;
    bool isInitialized;
    bool maintainAspectRatio;
    
    // Freeze detection and recovery
    QTimer *freezeDetectionTimer;
    QTimer *reconnectionTimer;
    qint64 lastFrameTime;
    int reconnectionAttempts;
    bool isReconnecting;
    
    // Static helper functions
    static gboolean onBusMessage(GstBus *bus, GstMessage *msg, gpointer user_data);
    static GstFlowReturn onNewSample(GstAppSink *appsink, gpointer user_data);
    
    // Private helper functions
    void initializeClient();
    void checkGStreamerPlugins();
    void setupTlsConfiguration();
    void startPipeline();
    GstElement* createOptimalDecoder();
    void configureElements(GstElement *queue, GstElement *h264parse);
    void setupBusMessageHandler();
    void setupOpenCL();
    
    // OpenCV processing functions
    cv::Mat processOpenCVFrame(const cv::Mat& inputFrame);
    QImage matToQImage(const cv::Mat& mat) const;
    void updateDisplayFrame(const QImage& qimage);
    
    // Freeze detection and recovery functions
    void setupFreezeDetection();
    void checkForFreeze();
    void handleStreamFreeze();
    void attemptReconnection();
    void resetPipeline();
};

#endif // RTSPCLIENTCAMERA1_H