#pragma once
#include <QMainWindow>
#include <QTabWidget>
#include <QGraphicsView>
#include <QGraphicsScene>
#include <QTableWidget>
#include <QLabel>
#include <QMutex>
#include <QHeaderView>
#include <QDockWidget>
#include <QVector>
#include <QWebEngineView>
#include <QWebChannel>
#include <QTimer>
#include <QGridLayout>
#include <QtCharts/QChartView>
#include <QtCharts/QChart>
#include <QtCharts/QLineSeries>
#include <QtCharts/QPieSeries>
#include <QProgressBar>
#include <QComboBox>
#include <QStackedWidget>
#include <QDateEdit>
#include <QListWidget>
#include <QPushButton>
#include <QAction>
#include <QToolBar>
#include <QGroupBox>
#include "databasemanager.h"
#include "microphone.h"

// Forward declarations
class RtspClient;
class RtspClientCamera1;

// VideoWidget: 카메라 스트림 + 오버레이 표시용
class VideoWidget : public QLabel {
    Q_OBJECT
public:
    explicit VideoWidget(QWidget* parent = nullptr);

    void setFrame(const QPixmap& pixmap);
    void setOverlays(const QVector<QRect>& rects);

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    QPixmap frame;
    QVector<QRect> overlays;
    QMutex  mutex;
};

// MapBridge: 지도와 Qt 간의 통신을 위한 클래스
class MapBridge : public QObject {
    Q_OBJECT

public slots:
    void updateCameraStatus(int cameraId, const QString& status);
    void setCameraLocations();
    void resetMapView();  // 지도를 기본 위치로 리셋

signals:
    void cameraStatusChanged(int cameraId, const QString& status);
    void mapResetRequested();
};

// MonitorWindow: 로그인 후 보여줄 메인 모니터링 UI
class MonitorWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MonitorWindow(const QString& userId, QWidget* parent = nullptr);
    ~MonitorWindow();
    void setCurrentUserId(const QString& id);
    void addLogEntry(const QString& source, const QString& event, const QString& status);
    void loadUserLogsFromDatabase(const QString& userId);
    void addFallEventEntry(const QString& eventTime, const QString& imagePath, const QString& jsonData);
    QPushButton* m_rescueEndButton = nullptr;  // 상황 종료 버튼
    QLabel* m_activeAlarmLabel = nullptr;  // Active Alarms

    // density
    int crowdPercent = 0;
    QProgressBar* m_crowdProgressBar = nullptr;
    QLabel* percentLabel = nullptr;
    void updateGateTotalCount();
    int getTodayOvercrowdedCount();
    // alarm
    void showCustomFallAlert(const QString& eventTime, const QString& imagePath, const QString& jsonData);
    QLabel* m_alertBannerLabel = nullptr;

public slots:
    void updateCameraCrowdCount(int cameraId, int count);

private slots:
    void startMonitoring();
    void stopMonitoring();
    void takeSnapshot();
    void refreshAll();
    // void updateDemo();
    // void updateCrowdStatus();  // MQTT에서 받은 정보로 지도 업데이트
    // ----- Pi camera control functions -----
    void startPiCamera2();
    void stopPiCamera2();
    void startPiCamera3();
    void stopPiCamera3();
    void startPiCamera4();
    void stopPiCamera4();
    void startAllPiCameras();
    void stopAllPiCameras();
    void onTabChanged(int index);
    // ----- logout and deleteaccount -----
    void logout();
    void deleteAccount();
    // ----- log -----
    void onSearchLogs();
    void onExportLogs();
    // ----- MIC -----
    void startStreaming();
    void stopStreaming();
    // ----- Header ----- //
    void updateTime();
    void toggleMic();

private:
    void createActions();
    void createMenus();
    //----- Header -----//
    QWidget* createHeaderBar();
    QLabel* timeLabel;
    QPushButton* micBtn;
    bool micOn = false;
    void updateMicState(bool on);

    void createStatusBar();
    QWidget* createCentralWidget();
    QWidget* createDockWidgets();
    void setupKakaoMap();
    int getTodayFallEventCount();  // 오늘 fall 이벤트 수 반환
    //----- Monitoring -----//
    QFrame* createEventCard(const QString& title, QLabel* countLabel, const QString& borderColor, const QString& numberColor);
    QWidget* createPeopleLabel(const QString& labelText, QLabel* countLabel, const QColor& color);
    QGroupBox* createPeopleBox(const QString& title, QLabel* countLabel, const QColor& color);
    //----- Setting -----//
    QWidget* createSettingTab();

    QWidget* MonitorWindowWidget;
    QWidget* CenterWidget;
    // Camera management functions
    void reloadCameraConfiguration();
    // Dashboard 탭을 위한 함수들
    QWidget* makeCard(const QString& title, QLabel* valueLabel);
    QWidget* createDashboardWidget();
    QWidget* createGraphTab();
    QLabel* m_fallEventLabel = nullptr;

    QVector<VideoWidget*> cameras;
    QVector<class RtspClient*> rtspClients;  // Track RTSP stream objects (cameras 2,3,4)
    class RtspClientCamera1* camera1Client;  // Specialized client for camera 1
    QAction       *startAct, *stopAct, *snapAct, *refreshAct;
    QTabWidget    *cameraTabs;
    QWebEngineView *mapView;  // Naver Map을 위한 웹뷰
    QWebChannel   *webChannel;
    MapBridge     *mapBridge;
    QTableWidget  *alertTable;
    QTableWidget  *persistentLogTable;
    QString m_currentUserId; // store logged-in user
    // ----- log search ----- //
    QComboBox* m_filterCombo = nullptr;
    QStackedWidget* m_inputStack = nullptr;
    QLineEdit* m_filterInput = nullptr;
    QDateEdit* m_fromDate = nullptr;
    QDateEdit* m_toDate = nullptr;
    QPushButton* m_refreshLogsButton = nullptr;  // Log History 새로고침 버튼
    // fall event log
    QVBoxLayout* scrollLayout = nullptr;
    // gate 좌표
    DatabaseManager::CameraCoordinates m_userCameraCoords;
    QLabel* m_cameraCoordLabel = nullptr;
    // MIC
    Microphone* microphone = nullptr;
    // QPushButton* start_button = nullptr;
    // QPushButton* stop_button = nullptr;

    // Today's event status
    int totalEvents;
    int todayFallCount;
    int todayOvercrowded;

    // determineStatusFromCrowdCount
    QString determineStatusFromCrowdCount(int count);
    int yellow = 15;
    int red = 30;

    // real time status
    QLabel* indoorCountLabel = nullptr;
    QLabel* gateCountLabel = nullptr;
    QLabel* gate1CountLabel = nullptr;
    QLabel* gate2CountLabel = nullptr;
    QLabel* gate3CountLabel = nullptr;

    // event log 팝업
    QMap<QWidget*, QString> m_cardEventTimes;
    void showEventDetailsPopup(const QString& eventTime);

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;
};
