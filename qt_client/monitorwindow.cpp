#include "mqtt.h"
#include <QMenuBar>
#include <QToolBar>
#include <QStatusBar>
#include <QSplitter>
#include <QDockWidget>
#include <QTimer>
#include <QPainter>
#include <QMutexLocker>
#include <QVBoxLayout>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QVector>
#include <QDateTime>
#include <QTableWidgetItem>
#include <QDir>
#include <QStandardPaths>
#include <QMessageBox>
#include <QThread>
#include <QWebEngineView>
#include <QWebEnginePage>
#include <QWebEngineSettings>
#include <QUrl>
#include <QDesktopServices>
#include <QFileInfo>
#include <QtCharts/QChartView>
#include <QtCharts/QChart>
#include <QtCharts/QLineSeries>
#include <QtCharts/QPieSeries>
#include <QtCharts/QPieSlice>
#include <QProgressBar>
#include <QLabel>
#include <QtGlobal>
#include <QSqlQuery>
#include <QSqlError>
#include <QFileDialog>
#include <QtCharts/QValueAxis>
#include <QListWidget>
#include <QListWidgetItem>
#include <QJsonParseError>
#include <QJsonDocument>
#include <QTextEdit>
#include "rtspclientcamera1.h"
#include "mainwindow.h"
#include "monitorwindow.h"
#include "rtspclient.h"
#include "rtspclientcamera1.h"
#include "databasemanager.h"

// VideoWidget
VideoWidget::VideoWidget(QWidget* parent)
    : QLabel(parent)
{
    setMinimumSize(320, 240);
    setAlignment(Qt::AlignCenter);
}

void VideoWidget::setFrame(const QPixmap& pixmap) {
    QMutexLocker lk(&mutex);
    frame = pixmap;
    update();
}

void VideoWidget::setOverlays(const QVector<QRect>& rects) {
    QMutexLocker lk(&mutex);
    overlays = rects;
    update();
}

void VideoWidget::paintEvent(QPaintEvent* event) {
    QLabel::paintEvent(event);
    QPainter p(this);
    QMutexLocker lk(&mutex);
    p.setPen(QPen(Qt::red, 2));
    for (const QRect& r : overlays) {
        p.drawRect(r);
    }
}

// MonitorWindow
MonitorWindow::MonitorWindow(const QString& userId, QWidget* parent)
    : QMainWindow(parent), m_currentUserId(userId)
    , camera1Client(nullptr)
{
    setWindowTitle("PIT_STOP");

    createActions();
    createMenus();
    // createStatusBar();

    MonitorWindowWidget = new QWidget(this);
    QVBoxLayout* MonitorWindowLayout = new QVBoxLayout(MonitorWindowWidget);
    MonitorWindowLayout->setContentsMargins(2, 2, 2, 2);
    MonitorWindowLayout->setSpacing(5);
    MonitorWindowLayout->addWidget(createHeaderBar());
    // MonitorWindowLayout->addWidget(createCentralWidget());
    QHBoxLayout* MainLayout = new QHBoxLayout;
    MainLayout->addWidget(createCentralWidget());
    MainLayout->addWidget(createDockWidgets()); // ㅠㅠ
    MonitorWindowLayout->addLayout(MainLayout);

    setCentralWidget(MonitorWindowWidget);

    setMinimumSize(1600, 900);  // Increase size more to prevent overlap
    resize(1600, 900);

    MonitorWindowWidget->setStyleSheet(R"(
        QWidget {
            background-color: #101828;
        }
    )");
    // MonitorWindowWidget->setStyleSheet(R"(
    //     QWidget {
    //         background-color: #1A1E2A;
    //         // border: 1px solid #303544;
    //         // border-radius: 10px;
    //     }
    // )");

    connect(cameraTabs, &QTabWidget::currentChanged, this, &MonitorWindow::onTabChanged);
}

void MonitorWindow::onTabChanged(int index)
{
    // Live View 탭으로 돌아왔을 때 RTSP 스트림이 확실히 PLAYING 상태가 되도록
    if (index == 0) {
        // Start camera 1 (specialized OpenCV client)
        if (camera1Client) {
            camera1Client->startStream();
        }

        // Start cameras 2, 3, 4 (regular clients)
        for (RtspClient* client : rtspClients) {
            if (client)
                client->startStream();
        }
    }
}

MonitorWindow::~MonitorWindow(){
    // Clean up specialized camera 1 client
    if (camera1Client) {
        delete camera1Client;
        camera1Client = nullptr;
    }

    // Proper cleanup for WebEngine
    if (mapView) {
        mapView->page()->deleteLater();
        mapView->deleteLater();
    }
    if (webChannel) {
        webChannel->deleteLater();
    }
    if (mapBridge) {
        mapBridge->deleteLater();
    }
}

void MonitorWindow::setCurrentUserId(const QString& id) {
    m_currentUserId = id;
    DatabaseManager db("users.db");
    // qDebug() << "[sequence] setcurrentuserid";

    this->setProperty("m_currentUserId", id);  // ✅ MQTT 로그 저장용
    qDebug() << "[SetUser] m_currentUserId set to:" << m_currentUserId;

    if (!db.getCameraCoordinates(id, m_userCameraCoords)) {
        qWarning() << "[getCameraCoordinates] 사용자 좌표 불러오기 실패";
    }
    if (m_cameraCoordLabel) {
        QString coordText = QString("Cam1: (%1, %2)\nCam2: (%3, %4)\nCam3: (%5, %6)")
        .arg(m_userCameraCoords.cam1x, 0, 'f', 1)
            .arg(m_userCameraCoords.cam1y, 0, 'f', 1)
            .arg(m_userCameraCoords.cam2x, 0, 'f', 1)
            .arg(m_userCameraCoords.cam2y, 0, 'f', 1)
            .arg(m_userCameraCoords.cam3x, 0, 'f', 1)
            .arg(m_userCameraCoords.cam3y, 0, 'f', 1);

        m_cameraCoordLabel->setText(coordText);
    }
}

void MonitorWindow::createActions() {
    startAct = new QAction(tr("▶️ Start"), this);
    stopAct = new QAction(tr("⏹️ Stop"), this);
    snapAct = new QAction(tr("📸 Snapshot"), this);
    refreshAct = new QAction(tr("🔃 Refresh"), this);

    connect(startAct, &QAction::triggered, this, &MonitorWindow::startMonitoring);
    connect(stopAct,  &QAction::triggered, this, &MonitorWindow::stopMonitoring);
    connect(snapAct,  &QAction::triggered, this, &MonitorWindow::takeSnapshot);
    connect(refreshAct,&QAction::triggered, this, &MonitorWindow::refreshAll);
}

void MonitorWindow::createMenus() {
    QMenu* cameras = menuBar()->addMenu(tr("Cameras"));

    // Individual camera controls
    QAction* startCam2Act = cameras->addAction(tr("▶️ Start Camera 2"));
    QAction* stopCam2Act = cameras->addAction(tr("⏹️ Stop Camera 2"));
    cameras->addSeparator();

    QAction* startCam3Act = cameras->addAction(tr("▶️ Start Camera 3"));
    QAction* stopCam3Act = cameras->addAction(tr("⏹️ Stop Camera 3"));
    cameras->addSeparator();

    QAction* startCam4Act = cameras->addAction(tr("▶️ Start Camera 4"));
    QAction* stopCam4Act = cameras->addAction(tr("⏹️ Stop Camera 4"));
    cameras->addSeparator();

    // Bulk controls
    QAction* startAllPiAct = cameras->addAction(tr("▶️ Start All Pi Cameras"));
    QAction* stopAllPiAct = cameras->addAction(tr("⏹️ Stop All Pi Cameras"));
    cameras->addSeparator();

    // Snapshot action
    cameras->addAction(snapAct);

    // Connect Pi camera actions
    connect(startCam2Act, &QAction::triggered, this, &MonitorWindow::startPiCamera2);
    connect(stopCam2Act, &QAction::triggered, this, &MonitorWindow::stopPiCamera2);
    connect(startCam3Act, &QAction::triggered, this, &MonitorWindow::startPiCamera3);
    connect(stopCam3Act, &QAction::triggered, this, &MonitorWindow::stopPiCamera3);
    connect(startCam4Act, &QAction::triggered, this, &MonitorWindow::startPiCamera4);
    connect(stopCam4Act, &QAction::triggered, this, &MonitorWindow::stopPiCamera4);
    connect(startAllPiAct, &QAction::triggered, this, &MonitorWindow::startAllPiCameras);
    connect(stopAllPiAct, &QAction::triggered, this, &MonitorWindow::stopAllPiCameras);
}

void MonitorWindow::logout() {
    QMessageBox::information(this, tr("Logout"), tr("You have been logged out."));
    MainWindow* login = new MainWindow();
    login->show();
    this->close();
}

void MonitorWindow::deleteAccount() {
    if (QMessageBox::question(this, tr("Delete Account"), tr("Are you sure you want to delete your account?")) == QMessageBox::Yes) {
        DatabaseManager db("users.db");
        if (db.deleteUser(m_currentUserId)) {
            QMessageBox::information(this, tr("Account Deleted"), tr("Your account has been removed."));
            MainWindow* login = new MainWindow();
            login->show();
            this->close();
        } else {
            QMessageBox::warning(this, tr("Error"), tr("Failed to delete account."));
        }
    }
}

void MonitorWindow::startStreaming()
{
    if (microphone)
        return;

    const QString ip = "192.168.0.115";
    const quint16 port = 8888;
    microphone = new Microphone(this);
    if (microphone->start(ip, port))
    {
        qDebug() << "[UI] Microphone streaming started.";
    }
    else
    {
        delete microphone;
        microphone = nullptr;
    }
}
// === Stops the audio streaming and disconnects ===
void MonitorWindow::stopStreaming()
{
    if (!microphone) return;
    microphone->stop();
    delete microphone;
    microphone = nullptr;
    qDebug() << "[UI] Microphone streaming stopped.";
}

void MonitorWindow::updateTime() {
    QString currentTime = QDateTime::currentDateTime().toString("AP hh:mm:ss");
    QString currentDate = QDate::currentDate().toString("yyyy. M. d.");
    timeLabel->setText(currentTime + "  |\n   " + currentDate + "  |");
}

void MonitorWindow::toggleMic() {
    micOn = !micOn;
    MonitorWindow::updateMicState(micOn);
}

void MonitorWindow::updateMicState(bool on) {
    if (on) {
        micBtn->setText("🎙 마이크 ON");
        micBtn->setStyleSheet(R"(
            QPushButton {
                color: #00ff99;
                border: 1px solid #00ff99;
                background-color: transparent;
            }
            QPushButton:hover {
                background-color: #004d3c;
            }
        )");
        MonitorWindow::startStreaming();
    } else {
        micBtn->setText("🎙 마이크 OFF");
        micBtn->setStyleSheet(R"(
            QPushButton {
                color: #cccccc;
                border: 1px solid #666666;
                background-color: transparent;
            }
            QPushButton:hover {
                background-color: #333333;
            }
        )");
        MonitorWindow::stopStreaming();
    }
}

QWidget* MonitorWindow::createHeaderBar() {
    QWidget* header = new QWidget;
    header->setStyleSheet("background-color: #1e2939; color: white;");
    // setStyleSheet(R"(
    //         QWidget {
    //             background-color: #1b2533;
    //             color: white;
    //         }
    //     )");
    setStyleSheet(R"(
            QPushButton {
                background-color: transparent;
                border: 1px solid #555;
                color: white;
                padding: 5px 12px;
                border-radius: 5px;
            }
            QPushButton:hover {
                background-color: #3a4a5f;
            }
        )");
    header->setFixedHeight(50);

    // 전체 레이아웃
    QHBoxLayout* layout = new QHBoxLayout(header);
    layout->setContentsMargins(10, 0, 10, 0); // 여백 설정
    layout->setSpacing(15); // 간격

    // 왼쪽: 시스템 아이콘 + 이름
    QLabel* iconLabel = new QLabel;
    iconLabel->setPixmap(QPixmap(":/new/prefix1/images/System_Name.png").scaled(24, 24, Qt::KeepAspectRatio, Qt::SmoothTransformation));

    QLabel* titleLabel = new QLabel("군중밀집 관리 시스템");
    titleLabel->setStyleSheet("font-weight: bold; font-size: 16px;");
    titleLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);

    QLabel* subTitleLabel = new QLabel("Emergency Crowd Management System");
    subTitleLabel->setStyleSheet("font-size: 10px; color: #bbb;");
    // subTitleLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);

    QVBoxLayout* titleLayout = new QVBoxLayout;
    titleLayout->setContentsMargins(0, 0, 0, 0);
    titleLayout->setSpacing(0);
    titleLayout->addWidget(titleLabel);
    titleLayout->addWidget(subTitleLabel);

    QHBoxLayout* leftLayout = new QHBoxLayout;
    leftLayout->setContentsMargins(0, 0, 0, 0);
    leftLayout->setSpacing(5);
    leftLayout->addWidget(iconLabel);
    leftLayout->addLayout(titleLayout);
    leftLayout->setSpacing(10);

    QWidget* leftWidget = new QWidget;
    leftWidget->setLayout(leftLayout);

    layout->addWidget(leftWidget);
    layout->addStretch();  // 중앙 여백

    // 오른쪽: 시간, 날짜, 이벤트, 경고, 마이크, 관리자, 로그아웃
    timeLabel = new QLabel;
    updateTime();
    QTimer* timer = new QTimer(this);
    connect(timer, &QTimer::timeout, this, &MonitorWindow::updateTime);
    timer->start(1000);

    QWidget* TodayWidget = new QWidget(header);
    QVBoxLayout* TodayLayout = new QVBoxLayout(TodayWidget);
    TodayLayout->setContentsMargins(0, 0, 0, 0);
    TodayLayout->setSpacing(0);
    TodayLayout->setAlignment(Qt::AlignCenter);
    // 🔸 숫자 라벨 (이벤트 수)
    todayFallCount = getTodayFallEventCount();
    QLabel* countLabel = new QLabel(QString::number(todayFallCount));
    countLabel->setStyleSheet(R"(
        QLabel {
            color: #FF6600;
            font-size: 18px;
            font-weight: bold;
        }
    )");
    countLabel->setAlignment(Qt::AlignCenter);
    // 🔸 텍스트 라벨 ("오늘 이벤트")
    QLabel* textLabel = new QLabel("오늘 이벤트");
    textLabel->setStyleSheet(R"(
        QLabel {
            color: #BBBBBB;
            font-size: 11px;
        }
    )");
    textLabel->setAlignment(Qt::AlignCenter);
    TodayLayout->addWidget(countLabel);
    TodayLayout->addWidget(textLabel);
    QTimer* fallCardTimer = new QTimer(this);
    connect(fallCardTimer, &QTimer::timeout, this, [=]() {
        if (countLabel && !m_currentUserId.isEmpty()) {
            int count = getTodayFallEventCount();
            todayFallCount = count;
            countLabel->setText(QString::number(count));
        }
    });
    fallCardTimer->start(1000);  // 1초 간격으로 fall count 갱신

    // QPushButton* alertBtn = new QPushButton("⚠ 밀집도 경고");
    // alertBtn->setStyleSheet("color: #f0c000; border: 1px solid #f0c000; background-color: transparent; border-radius: 4px;");
    QPushButton* alertBtn = new QPushButton("✅ 밀집도 양호");
    alertBtn->setStyleSheet("color: #00c853; border: 1px solid #00c853; background-color: transparent; border-radius: 4px;");

    micBtn = new QPushButton;
    micBtn->setCursor(Qt::PointingHandCursor);  // 마우스 포인터 변경
    updateMicState(false);  // 초기 상태 OFF
    connect(micBtn, &QPushButton::clicked, this, &MonitorWindow::toggleMic);

    // ----- 상황 종료 ----- //
    m_rescueEndButton = new QPushButton("🚨 상황 종료");
    m_rescueEndButton->setEnabled(false);  // 초기에는 비활성화
    m_rescueEndButton->setStyleSheet(R"(
        QPushButton:enabled {
            background-color: #c0392b;
            color: white;
            font-weight: bold;
            border: none;
            border-radius: 8px;
            padding: 8px 16px;
        }
        QPushButton:enabled:hover {
            background-color: #e74c3c;
        }
        QPushButton:disabled {
            background-color: #7f8c8d;
            color: #cccccc;
        }
    )");
    connect(m_rescueEndButton, &QPushButton::clicked, this, [=]() {
        if (auto* mqtt = findChild<Mqtt*>()) {
            mqtt->publishMessage("qt/off", "RESCUE_END");
        }
        m_rescueEndButton->setEnabled(false);  // 버튼 비활성화
        addLogEntry("User", "Rescue End", "Rescue operation manually ended");

        QMessageBox::information(this, "상황 종료", "상황 종료 신호를 전송했습니다.");

        if (m_activeAlarmLabel) {
            m_activeAlarmLabel->setText("✅ No Fall Detected");
            m_activeAlarmLabel->setStyleSheet("color: green; font-size: 18px; font-weight: bold;");
        }

        if (m_alertBannerLabel) {
            m_alertBannerLabel->deleteLater();
            m_alertBannerLabel = nullptr;
        }
    });

    QLabel* adminIcon = new QLabel;
    adminIcon->setPixmap(QPixmap(":/new/prefix1/images/User.png").scaled(16, 16, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    QLabel* adminLabel = new QLabel;
    adminLabel->setText(QString("%1").arg(m_currentUserId));
    adminLabel->setStyleSheet("color: #ccc;");
    QHBoxLayout* adminLayout = new QHBoxLayout;
    adminLayout->setContentsMargins(0, 0, 0, 0);
    adminLayout->setSpacing(5);
    adminLayout->addWidget(adminIcon);
    adminLayout->addWidget(adminLabel);
    QPushButton* logoutBtn = new QPushButton(QIcon(":/new/prefix1/images/Logout_.png"), "로그아웃");
    logoutBtn->setCursor(Qt::PointingHandCursor);  // 마우스 포인터 변경
    // 🔸 기본 스타일 설정
    logoutBtn->setStyleSheet(R"(
        QPushButton {
            background-color: transparent;
            color: #cccccc;
            border: 1px solid #666666;
            padding: 5px 10px;
            border-radius: 5px;
        }
        QPushButton:hover {
            background-color: #e74c3c;     /* 빨간 배경 */
            color: white;
            border: 1px solid #e74c3c;
        }
    )");
    // 🔸 클릭 시 로그아웃 동작 연결
    connect(logoutBtn, &QPushButton::clicked, this, [=]() {
        if (MonitorWindow* mw = qobject_cast<MonitorWindow*>(this->window())) {
            mw->logout();  // MonitorWindow의 함수 호출
        }
    });

    QWidget* adminWidget = new QWidget;
    adminWidget->setLayout(adminLayout);

    layout->addWidget(timeLabel);
    layout->addWidget(TodayWidget);
    layout->addWidget(alertBtn);
    layout->addWidget(micBtn);
    layout->addWidget(m_rescueEndButton);
    layout->addWidget(adminWidget);
    layout->addWidget(logoutBtn);

    return header;
}

QWidget* MonitorWindow::createCentralWidget() {
    QWidget* central = new QWidget(this);
    auto* cameraLayout = new QGridLayout(central);
    cameraLayout->setSpacing(2);  // Small gaps like original
    cameraLayout->setContentsMargins(4,4,4,4);
    central->setLayout(cameraLayout);

    // RTSP URLs for the 4 cameras
    QVector<QString> rtsp_urls;
    rtsp_urls.push_back("rtsp://admin:veda1357!@192.168.0.13:554/0/profile2/media.smp"); // Camera 1 - Working Hanwha
    rtsp_urls.push_back("rtsps://192.168.0.62:8555/stream");   // Camera 2 - Pi Camera (moved from Camera 4)
    rtsp_urls.push_back("rtsps://192.168.0.58:8555/stream");   // Camera 3 - Pi Camera
    rtsp_urls.push_back("rtsps://192.168.0.83:8555/stream");   // Camera 4 - Pi Camera

    // Create camera widgets with labels like project_3
    // Layout: 2x3 grid where Camera 1 spans 2 columns (larger)
    // Camera 1 (192.168.0.13) - spans 2 columns, top row - Using specialized OpenCV client
    auto* box1 = new QGroupBox(tr("Indoor"), central);
    box1->setStyleSheet(R"(
        QGroupBox {
            color: white;                        /* 제목 글씨 색 */
            border: 1px solid #444;             /* 테두리 색 */
            border-radius: 10px;                /* 둥근 테두리 */
            margin-top: 8px;                    /* 제목과 내용 간 여백 */
            padding: 8px;
            font-weight: bold;
            font-size: 14px;
        }
        QGroupBox::title {
            subcontrol-origin: margin;
            subcontrol-position: top left;      /* 제목 위치 */
            padding-left: 12px;
            padding-top: 1px;
        }
    )");
    auto* layout1 = new QVBoxLayout(box1);
    layout1->setContentsMargins(2,2,2,2);
    layout1->setSpacing(2);
    camera1Client = new RtspClientCamera1(rtsp_urls[0]);
    camera1Client->setMinimumSize(500, 300);  // Reduced from 640x360 to fit better
    camera1Client->setStyleSheet("border: 2px solid #333; background-color: black;");
    layout1->addWidget(camera1Client);
    camera1Client->setDistortionParameters(
        -0.2, 0.05, 0.0,     // k1, k2, k3 (radial distortion coefficients - milder)
        0.001, -0.001,      // p1, p2 (tangential distortion coefficients)
        960, 960,           // fx, fy (focal length - for 1920x1080 resolution)
        960, 540            // cx, cy (principal point - image center)
        );
    camera1Client->enableDistortionCorrection(true);
    cameraLayout->addWidget(box1, 0, 0, 1, 2);  // row 0, col 0-1 (span 2 cols)

    // Camera 2 - top right
    auto* box2 = new QGroupBox(tr("Gate 1"), central);
    box2->setStyleSheet(R"(
        QGroupBox {
            color: white;                        /* 제목 글씨 색 */
            border: 1px solid #444;             /* 테두리 색 */
            border-radius: 10px;                /* 둥근 테두리 */
            margin-top: 8px;                    /* 제목과 내용 간 여백 */
            padding: 8px;
            font-weight: bold;
            font-size: 14px;
        }
        QGroupBox::title {
            subcontrol-origin: margin;
            subcontrol-position: top left;      /* 제목 위치 */
            padding-left: 12px;
            padding-top: 1px;
        }
    )");
    auto* layout2 = new QVBoxLayout(box2);
    layout2->setContentsMargins(2,2,2,2);
    layout2->setSpacing(2);
    RtspClient* stream2 = new RtspClient(rtsp_urls[1]);
    stream2->setMinimumSize(350, 240);  // Reduced size
    stream2->setStyleSheet("border: 2px solid #333; background-color: black;");
    layout2->addWidget(stream2);
    rtspClients.append(stream2);  // Store for monitoring control
    cameraLayout->addWidget(box2, 0, 2);  // row 0, col 2

    // Camera 3 - bottom left
    auto* box3 = new QGroupBox(tr("Gate 2"), central);
    box3->setStyleSheet(R"(
        QGroupBox {
            color: white;                        /* 제목 글씨 색 */
            border: 1px solid #444;             /* 테두리 색 */
            border-radius: 10px;                /* 둥근 테두리 */
            margin-top: 8px;                    /* 제목과 내용 간 여백 */
            padding: 8px;
            font-weight: bold;
            font-size: 14px;
        }
        QGroupBox::title {
            subcontrol-origin: margin;
            subcontrol-position: top left;      /* 제목 위치 */
            padding-left: 12px;
            padding-top: 1px;
        }
    )");
    auto* layout3 = new QVBoxLayout(box3);
    layout3->setContentsMargins(2,2,2,2);
    layout3->setSpacing(2);
    RtspClient* stream3 = new RtspClient(rtsp_urls[2]);
    stream3->setMinimumSize(350, 240);  // Reduced size
    stream3->setStyleSheet("border: 2px solid #333; background-color: black;");
    layout3->addWidget(stream3);
    rtspClients.append(stream3);  // Store for monitoring control
    cameraLayout->addWidget(box3, 1, 0);  // row 1, col 0

    // Camera 4 - bottom center
    auto* box4 = new QGroupBox(tr("Gate 3"), central);
    box4->setStyleSheet(R"(
        QGroupBox {
            color: white;                        /* 제목 글씨 색 */
            border: 1px solid #444;             /* 테두리 색 */
            border-radius: 10px;                /* 둥근 테두리 */
            margin-top: 8px;                    /* 제목과 내용 간 여백 */
            padding: 8px;
            font-weight: bold;
            font-size: 14px;
        }
        QGroupBox::title {
            subcontrol-origin: margin;
            subcontrol-position: top left;      /* 제목 위치 */
            padding-left: 12px;
            padding-top: 1px;
        }
    )");
    auto* layout4 = new QVBoxLayout(box4);
    layout4->setContentsMargins(2,2,2,2);
    layout4->setSpacing(2);
    RtspClient* stream4 = new RtspClient(rtsp_urls[3]);
    stream4->setMinimumSize(350, 240);  // Reduced size
    stream4->setStyleSheet("border: 2px solid #333; background-color: black;");
    layout4->addWidget(stream4);
    rtspClients.append(stream4);  // Store for monitoring control
    cameraLayout->addWidget(box4, 1, 1);  // row 1, col 1

    // Kakao Map area - bottom right (moved from dock widget)
    auto* mapBox = new QGroupBox(tr("Camera Location Map"), central);
    mapBox->setStyleSheet(R"(
        QGroupBox {
            color: white;                        /* 제목 글씨 색 */
            border: 1px solid #444;             /* 테두리 색 */
            border-radius: 10px;                /* 둥근 테두리 */
            margin-top: 8px;                    /* 제목과 내용 간 여백 */
            padding: 8px;
            font-weight: bold;
            font-size: 14px;
        }
        QGroupBox::title {
            subcontrol-origin: margin;
            subcontrol-position: top left;      /* 제목 위치 */
            padding-left: 12px;
            padding-top: 1px;
        }
    )");
    auto* mapLayout = new QVBoxLayout(mapBox);
    mapLayout->setContentsMargins(2, 2, 2, 2);
    mapLayout->setSpacing(2);

    // Create the map view here instead of in dock widget
    mapView = new QWebEngineView(mapBox);
    mapView->setMinimumSize(350, 240);
    mapView->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    // Enable debugging and configure WebEngine settings
    QWebEngineSettings *settings = mapView->page()->settings();
    settings->setAttribute(QWebEngineSettings::JavascriptEnabled, true);
    settings->setAttribute(QWebEngineSettings::LocalContentCanAccessRemoteUrls, true);
    settings->setAttribute(QWebEngineSettings::ErrorPageEnabled, true);
    settings->setAttribute(QWebEngineSettings::PluginsEnabled, true);

    mapLayout->addWidget(mapView);
    cameraLayout->addWidget(mapBox, 1, 2);  // row 1, col 2

    // Setup Kakao Map
    setupKakaoMap();

    qDebug() << "[DEBUG] mapView visible?:" << mapView->isVisible();
    qDebug() << "[DEBUG] mapView geometry:" << mapView->geometry();
    qDebug() << "[DEBUG] mapView sizeHint:" << mapView->sizeHint();
    qDebug() << "[MonitorWindow] mapView parent:" << mapView->parentWidget();
    qDebug() << "[MonitorWindow] isHidden:" << mapView->isHidden();
    qDebug() << "[MonitorWindow] isEnabled:" << mapView->isEnabled();

    // Set column and row stretches for better proportions
    cameraLayout->setColumnStretch(0, 1);
    cameraLayout->setColumnStretch(1, 1);
    cameraLayout->setColumnStretch(2, 1);
    cameraLayout->setRowStretch(0, 1);
    cameraLayout->setRowStretch(1, 1);

    // statusBar()->addPermanentWidget(new QLabel(tr("Cameras: 4")));

    return central;
}

// QWidget* MonitorWindow::createGraphTab() {
//     QWidget* widget = new QWidget;
//     widget->setStyleSheet("background-color: #1e2939; color: white;");

//     QVBoxLayout* GraphVbox = new QVBoxLayout(widget);
//     GraphVbox->setContentsMargins(2,2,2,2);
//     GraphVbox->setSpacing(5);

//     auto* GraphBox = new QGroupBox(widget);
//     GraphBox->setTitle("");
//     GraphBox->setStyleSheet("QGroupBox { border: 1px solid #444; border-radius: 8px; margin-top: 10px; padding: 6px; }");
//     GraphBox->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
//     auto* GraphLayout = new QVBoxLayout(GraphBox);
//     GraphLayout->setContentsMargins(2, 2, 2, 2);
//     GraphLayout->setSpacing(2);
//     GraphVbox->addWidget(GraphBox, /*stretch=*/1);

//     // DataManagement Title
//     QLabel *iconGraphLabel = new QLabel;
//     QLabel *titleGraphLabel = new QLabel("Real-time crowd count graph");
//     iconGraphLabel->setPixmap(QPixmap(":/new/prefix1/images/Graph.png").scaled(16, 16, Qt::KeepAspectRatio, Qt::SmoothTransformation));
//     QHBoxLayout *GraphTitleLayout = new QHBoxLayout;
//     GraphTitleLayout->addWidget(iconGraphLabel);
//     GraphTitleLayout->addWidget(titleGraphLabel);
//     GraphTitleLayout->addStretch();
//     GraphLayout->addLayout(GraphTitleLayout);

//     QLineSeries* s1 = new QLineSeries;  // Camera 1
//     QLineSeries* s2 = new QLineSeries;  // Camera 2
//     QLineSeries* s3 = new QLineSeries;  // Camera 3
//     QLineSeries* s4 = new QLineSeries;  // Camera 4
//     QList<QLineSeries*> seriesList = { s1, s2, s3, s4 };
//     // QStringList sources = { "Camera 1", "Camera 2", "Camera 3", "Camera 4" };
//     QStringList sources = { "Indoor", "Gate 1", "Gate 2", "Gate 3" };

//     int intervalSeconds = 5;  // 5초 간격
//     int numPoints = 60;       // 총 5분 (12 * 5초 * 5)

//     QChart* lineChart = new QChart;
//     lineChart->removeSeries(s1);
//     lineChart->removeSeries(s2);
//     lineChart->removeSeries(s3);
//     lineChart->removeSeries(s4);
//     lineChart->addSeries(s1);
//     lineChart->addSeries(s2);
//     lineChart->addSeries(s3);
//     lineChart->addSeries(s4);
//     lineChart->update();
//     s1->setName("Indoor"); // 시리즈 이름 설정
//     s2->setName("Gate 1");
//     s3->setName("Gate 2");
//     s4->setName("Gate 3");
//     auto* axisX = new QValueAxis;
//     axisX->setTitleText("Time (sec)");
//     axisX->setLabelFormat("%d");
//     axisX->setTickCount(15);  // 그래프 사이사이
//     axisX->setRange(0, intervalSeconds * (numPoints - 1));  // 예: 0~55
//     auto* axisY = new QValueAxis;
//     axisY->setTitleText("People Count");
//     axisY->setLabelFormat("%d");  // 정수로!
//     axisY->setTickCount(10);
//     axisY->setRange(0, 100);       // 기본 0~100 (값에 따라 자동 조정 가능)
//     for (auto* s : seriesList) {
//         lineChart->setAxisX(axisX, s);
//         lineChart->setAxisY(axisY, s);
//     }
//     lineChart->legend()->setAlignment(Qt::AlignBottom);
//     lineChart->setTitle("Trends in the number of people per gate over the past 5 minutes");

//     auto* lineChartView = new QChartView(lineChart);
//     lineChartView->setRenderHint(QPainter::Antialiasing);
//     lineChartView->setMinimumHeight(250);
//     lineChartView->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
//     // grid->addWidget(lineChartView, 1, 2, 2, 2);
//     GraphLayout->addWidget(lineChartView);

//     // 5초마다 그래프 자동 업데이트
//     QTimer* graphUpdateTimer = new QTimer(this);
//     connect(graphUpdateTimer, &QTimer::timeout, this, [=]() {
//         QDateTime now = QDateTime::currentDateTime();
//         for (auto* s : seriesList) s->clear();

//         QSqlQuery query(QSqlDatabase::database());

//         for (int i = 0; i < numPoints; ++i) {
//             QDateTime from = now.addSecs(-intervalSeconds * (numPoints - i));
//             QDateTime to   = from.addSecs(intervalSeconds);

//             for (int j = 0; j < sources.size(); ++j) {
//                 query.prepare(R"(
//                 SELECT status FROM logs
//                 WHERE user_id = :uid AND source = :src
//                 AND event = 'Number of people'
//                 AND timestamp BETWEEN :from AND :to
//                 ORDER BY timestamp DESC
//                 LIMIT 1
//             )");
//                 query.bindValue(":uid", m_currentUserId);
//                 query.bindValue(":src", sources[j]);
//                 query.bindValue(":from", from.toString("yyyy-MM-dd hh:mm:ss"));
//                 query.bindValue(":to",   to.toString("yyyy-MM-dd hh:mm:ss"));

//                 int count = 0;
//                 // bool parsed = false;
//                 if (query.exec() && query.next()) {
//                     QString status = query.value(0).toString();
//                     // QRegularExpression rx("Updated count:\\s*(\\d+)");
//                     QRegularExpression rx("Updated count[^\\d]*(\\d+)");
//                     QRegularExpressionMatch match = rx.match(status);
//                     if (match.hasMatch()) {
//                         count = match.captured(1).toInt();
//                     }
//                 }

//                 seriesList[j]->append(i * intervalSeconds, count);
//             }
//         }

//         lineChart->update();
//         lineChartView->repaint();  // 가장 확실한 강제 그리기
//         lineChartView->update();
//     });
//     graphUpdateTimer->start(1000);  // 5초마다 최신 그래프로 갱신

//     return widget;
// }

QWidget* MonitorWindow::createGraphTab() {
    QWidget* widget = new QWidget;
    widget->setStyleSheet("background-color: #1e2939; color: white;");

    QVBoxLayout* GraphVbox = new QVBoxLayout(widget);
    GraphVbox->setContentsMargins(2,2,2,2);
    GraphVbox->setSpacing(5);

    auto* GraphBox = new QGroupBox(widget);
    GraphBox->setTitle("");
    GraphBox->setStyleSheet("QGroupBox { border: 1px solid #444; border-radius: 8px; margin-top: 10px; padding: 6px; }");
    GraphBox->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    auto* GraphLayout = new QVBoxLayout(GraphBox);
    GraphLayout->setContentsMargins(2, 2, 2, 2);
    GraphLayout->setSpacing(2);
    GraphVbox->addWidget(GraphBox, /*stretch=*/1);

    // DataManagement Title
    QLabel *iconGraphLabel = new QLabel;
    QLabel *titleGraphLabel = new QLabel("Real-time crowd count graph");
    iconGraphLabel->setPixmap(QPixmap(":/new/prefix1/images/Graph.png").scaled(16, 16, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    QHBoxLayout *GraphTitleLayout = new QHBoxLayout;
    GraphTitleLayout->addWidget(iconGraphLabel);
    GraphTitleLayout->addWidget(titleGraphLabel);
    GraphTitleLayout->addStretch();
    GraphLayout->addLayout(GraphTitleLayout);

    // Create TabWidget for different views
    QTabWidget* graphTabWidget = new QTabWidget;
    graphTabWidget->setTabPosition(QTabWidget::North);
    graphTabWidget->setStyleSheet(R"(
        QTabWidget::pane {
            border: none;
            background-color: #1B2533;
        }

        QTabBar {
            background-color: #1B2533;
        }

        QTabBar::tab {
            background: transparent;
            color: #999999;
            padding: 6px 18px;
            font-size: 13px;
            font-weight: 500;
            border: 1px solid transparent;
            border-radius: 12px;
            margin-right: 6px;
        }

        QTabBar::tab:selected {
            background-color: #ffffff;
            color: #1B2533;
            font-weight: bold;
            border: 1px solid #cccccc;
        }

        QTabBar::tab:hover {
            color: #dddddd;
        }
    )");
    GraphLayout->addWidget(graphTabWidget);

    QStringList sources = { "Indoor", "Gate 1", "Gate 2", "Gate 3" };
    QStringList names = { "Indoor", "Gate 1", "Gate 2", "Gate 3" };
    int intervalSeconds = 5;  // 5초 간격
    int numPoints = 60;       // 총 5분 (12 * 5초 * 5)

    // All 탭 생성 (기존 4개 라인 차트)
    QWidget* allTabWidget = new QWidget;
    QVBoxLayout* allLayout = new QVBoxLayout(allTabWidget);

    QLineSeries* s1 = new QLineSeries;  // Camera 1
    QLineSeries* s2 = new QLineSeries;  // Camera 2
    QLineSeries* s3 = new QLineSeries;  // Camera 3
    QLineSeries* s4 = new QLineSeries;  // Camera 4
    QList<QLineSeries*> allSeriesList = { s1, s2, s3, s4 };

    QChart* allChart = new QChart;
    allChart->addSeries(s1);
    allChart->addSeries(s2);
    allChart->addSeries(s3);
    allChart->addSeries(s4);
    s1->setName("Indoor");
    s2->setName("Gate 1");
    s3->setName("Gate 2");
    s4->setName("Gate 3");

    // 범례 색상 명시적으로 지정
    s1->setColor(QColor("#3399ff"));  // 파랑
    s2->setColor(QColor("#99cc66"));  // 연두
    s3->setColor(QColor("#ffaa33"));  // 주황
    s4->setColor(QColor("#ff69b4"));  // 분홍

    auto* allAxisX = new QValueAxis;
    allAxisX->setTitleText("Time (sec)");
    allAxisX->setLabelFormat("%d");
    allAxisX->setTickCount(15);
    allAxisX->setRange(0, intervalSeconds * (numPoints - 1));  // 예: 0~295
    auto* allAxisY = new QValueAxis;
    allAxisY->setTitleText("People Count");
    allAxisY->setLabelFormat("%d");
    allAxisY->setTickCount(10);
    allAxisY->setRange(0, 100);
    for (auto* s : allSeriesList) {
        allChart->setAxisX(allAxisX, s);
        allChart->setAxisY(allAxisY, s);
    }
    allChart->legend()->setAlignment(Qt::AlignBottom);
    allChart->setTitle("Trends in the number of people per gate over the past 5 minutes");

    // All 차트 스타일 설정
    allChart->setBackgroundBrush(QBrush(QColor("#1B2533")));
    allChart->setTitleBrush(QBrush(Qt::white));
    allChart->legend()->setLabelColor(Qt::white);
    allChart->setPlotAreaBackgroundBrush(QBrush(QColor("#141B2B")));
    allChart->setPlotAreaBackgroundVisible(true);

    // 축 텍스트 색상 설정
    allAxisX->setLabelsBrush(QBrush(Qt::white));
    allAxisX->setTitleBrush(QBrush(Qt::white));
    allAxisX->setLinePenColor(QColor("#555"));
    allAxisX->setGridLineColor(QColor("#333"));
    allAxisY->setLabelsBrush(QBrush(Qt::white));
    allAxisY->setTitleBrush(QBrush(Qt::white));
    allAxisY->setLinePenColor(QColor("#555"));
    allAxisY->setGridLineColor(QColor("#333"));

    auto* allChartView = new QChartView(allChart);
    allChartView->setRenderHint(QPainter::Antialiasing);
    allChartView->setMinimumHeight(250);
    allChartView->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    allChartView->setStyleSheet("background-color: #1B2533; border: none;");
    allLayout->addWidget(allChartView);

    graphTabWidget->addTab(allTabWidget, "All");

    // 개별 탭들 생성
    QList<QLineSeries*> individualSeriesList;
    QList<QChart*> individualChartsList;
    QList<QChartView*> individualChartViewsList;
    QList<QColor> seriesColors = {
        QColor("#3399ff"),  // 파랑 - Indoor
        QColor("#99cc66"),  // 연두 - Gate 1
        QColor("#ffaa33"),  // 주황 - Gate 2
        QColor("#ff69b4")   // 분홍 - Gate 3
    };

    for (int i = 0; i < 4; ++i) {
        QWidget* individualTab = new QWidget;
        QVBoxLayout* individualLayout = new QVBoxLayout(individualTab);

        QLineSeries* series = new QLineSeries;
        series->setName(names[i]);
        series->setColor(seriesColors[i]);  // 각 개별 차트에도 동일한 색상 적용
        individualSeriesList.append(series);

        QChart* chart = new QChart;
        chart->addSeries(series);
        chart->setTitle(QString("%1 People Count Trend").arg(names[i]));

        auto* axisX = new QValueAxis;
        axisX->setTitleText("Time (sec)");
        axisX->setLabelFormat("%d");
        axisX->setTickCount(15);
        axisX->setRange(0, intervalSeconds * (numPoints - 1));  // 예: 0~295
        auto* axisY = new QValueAxis;
        axisY->setTitleText("People Count");
        axisY->setLabelFormat("%d");
        axisY->setTickCount(10);
        axisY->setRange(0, 100);

        chart->setAxisX(axisX, series);
        chart->setAxisY(axisY, series);
        chart->legend()->setAlignment(Qt::AlignBottom);

        // 개별 차트 스타일 설정
        chart->setBackgroundBrush(QBrush(QColor("#1B2533")));
        chart->setTitleBrush(QBrush(Qt::white));
        chart->legend()->setLabelColor(Qt::white);
        chart->setPlotAreaBackgroundBrush(QBrush(QColor("#141B2B")));
        chart->setPlotAreaBackgroundVisible(true);

        // 축 텍스트 색상 설정
        axisX->setLabelsBrush(QBrush(Qt::white));
        axisX->setTitleBrush(QBrush(Qt::white));
        axisX->setLinePenColor(QColor("#555"));
        axisX->setGridLineColor(QColor("#333"));
        axisY->setLabelsBrush(QBrush(Qt::white));
        axisY->setTitleBrush(QBrush(Qt::white));
        axisY->setLinePenColor(QColor("#555"));
        axisY->setGridLineColor(QColor("#333"));

        individualChartsList.append(chart);

        auto* chartView = new QChartView(chart);
        chartView->setRenderHint(QPainter::Antialiasing);
        chartView->setMinimumHeight(250);
        chartView->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        chartView->setStyleSheet("background-color: #1B2533; border: none;");
        individualChartViewsList.append(chartView);

        individualLayout->addWidget(chartView);
        graphTabWidget->addTab(individualTab, names[i]);
    }

    // 5초마다 그래프 자동 업데이트
    QTimer* graphUpdateTimer = new QTimer(this);
    connect(graphUpdateTimer, &QTimer::timeout, this, [=]() {
        QDateTime now = QDateTime::currentDateTime();
        QDateTime fiveMinutesAgo = now.addSecs(-300); // 5분 전

        // All 탭 시리즈 클리어
        for (auto* s : allSeriesList) s->clear();
        // 개별 탭 시리즈 클리어
        for (auto* s : individualSeriesList) s->clear();

        QSqlQuery query(QSqlDatabase::database());

        for (int i = 0; i < numPoints; ++i) {
            QDateTime from = now.addSecs(-intervalSeconds * (numPoints - i));
            QDateTime to   = from.addSecs(intervalSeconds);

            for (int j = 0; j < sources.size(); ++j) {
                query.prepare(R"(
                SELECT status FROM logs
                WHERE user_id = :uid AND source = :src
                AND event = 'Number of people'
                AND timestamp BETWEEN :from AND :to
                ORDER BY timestamp DESC
                LIMIT 1
            )");
                query.bindValue(":uid", m_currentUserId);
                query.bindValue(":src", sources[j]);
                query.bindValue(":from", from.toString("yyyy-MM-dd hh:mm:ss"));
                query.bindValue(":to",   to.toString("yyyy-MM-dd hh:mm:ss"));

                int count = 0;
                if (query.exec() && query.next()) {
                    QString status = query.value(0).toString();
                    QRegularExpression rx("Updated count[^\\d]*(\\d+)");
                    QRegularExpressionMatch match = rx.match(status);
                    if (match.hasMatch()) {
                        count = match.captured(1).toInt();
                    }
                }

                allSeriesList[j]->append(i * intervalSeconds, count);
                individualSeriesList[j]->append(i * intervalSeconds, count);
            }
        }

        allChart->update();
        allChartView->repaint();
        allChartView->update();

        for (int i = 0; i < individualChartsList.size(); ++i) {
            individualChartsList[i]->update();
            individualChartViewsList[i]->repaint();
            individualChartViewsList[i]->update();
        }
    });
    graphUpdateTimer->start(5000);  // 1초마다 최신 그래프로 갱신

    return widget;
}

void MonitorWindow::addFallEventEntry(const QString& eventTime, const QString& imagePath, const QString& jsonData) {
    if (!scrollLayout) return;

    // 카드 생성
    QFrame* card = new QFrame;
    card->setObjectName("eventCard");
    card->setStyleSheet(R"(
        #eventCard {
            background-color: #2a3545;
            border: 1px solid #4a5565;
            border-radius: 8px;
        })");

    QVBoxLayout* cardLayout = new QVBoxLayout(card);
    cardLayout->setContentsMargins(10, 8, 10, 8);
    cardLayout->setSpacing(4);

    // 1. 뱃지 영역
    QLabel* fallBadge = new QLabel("🔺 Fall Detection");
    QLabel* solvedBadge = new QLabel("🟢 Solved");

    fallBadge->setStyleSheet(R"(
        QLabel {
            background-color: #543342;
            color: #ff6467;
            border: 1px solid #ff6467;
            border-radius: 6px;
            padding: 2px 6px;
        })");
    solvedBadge->setStyleSheet(R"(
        QLabel {
            background-color: #27ae60;
            color: white;
            border: 1px solid white;
            border-radius: 6px;
            padding: 2px 6px;
        })");

    QHBoxLayout* statusLayout = new QHBoxLayout;
    statusLayout->addWidget(fallBadge);
    statusLayout->addWidget(solvedBadge);
    statusLayout->addStretch();

    // 2. 텍스트 정보
    QString cameraText = "Camera: Indoor";
    QString pathText = "최적 경로: -";

    // ✅ JSON 파싱
    QJsonParseError err;
    QJsonDocument doc = QJsonDocument::fromJson(jsonData.toUtf8(), &err);
    if (err.error == QJsonParseError::NoError && doc.isObject()) {
        QJsonObject obj = doc.object();

        if (obj.contains("min_gate_idx") && obj["min_gate_idx"].isDouble()) {
            int gateIdx = obj["min_gate_idx"].toInt();
            if (gateIdx >= 1 && gateIdx <= 3) {
                pathText = QString("최적 경로: Gate %1").arg(gateIdx);
            }
        }
    }

    QLabel* cameraLabel = new QLabel(cameraText);
    QLabel* pathLabel = new QLabel(pathText);
    QLabel* timeLabel = new QLabel(QDateTime::fromString(eventTime, "yyyyMMdd_hhmmss").toString("yyyy-MM-dd hh:mm:ss"));

    cameraLabel->setStyleSheet("background-color: #2a3545; color: white; font-weight: bold;");
    pathLabel->setStyleSheet("background-color: #2a3545; color: #51a2e1;");
    timeLabel->setStyleSheet("background-color: #2a3545; color: #aaa; font-size: 11px;");

    // 3. 레이아웃 구성
    cardLayout->addLayout(statusLayout);
    cardLayout->addWidget(cameraLabel);
    cardLayout->addWidget(pathLabel);
    cardLayout->addWidget(timeLabel);

    scrollLayout->insertWidget(0, card);  // 최신 카드가 위로

    // 이벤트 필터 등록 및 맵에 저장
    card->installEventFilter(this);
    m_cardEventTimes[card] = eventTime;
}

int MonitorWindow::getTodayFallEventCount() {
    QSqlQuery query(QSqlDatabase::database());
    query.prepare(R"(
        SELECT COUNT(*) FROM logs
        WHERE event = 'Fall Detected'
        AND DATE(timestamp) = DATE('now', 'localtime')
        AND user_id = ?
    )");
    query.addBindValue(m_currentUserId);
    if (query.exec() && query.next()) {
        return query.value(0).toInt();
    }
    qDebug() << "[FallCount] user_id =" << m_currentUserId;
    qDebug() << "[FallCount] count =" << query.value(0).toInt();

    return 0;
}


// QWidget* MonitorWindow::makeCard(const QString& title, const QString& value) {
//     QFrame* card = new QFrame;
//     card->setFrameShape(QFrame::StyledPanel);
//     card->setStyleSheet(R"(
//         QFrame { background: #ffffff; border-radius: 8px; }
//     )");
//     auto* v = new QVBoxLayout(card);
//     auto* t = new QLabel(title, card);
//     t->setStyleSheet("color:#555555; font-size:14px;");
//     auto* val = new QLabel(value, card);
//     val->setStyleSheet("font-size:24px; font-weight:bold;");
//     v->addWidget(t, 0, Qt::AlignLeft);
//     v->addWidget(val, 0, Qt::AlignCenter);
//     return card;
// }

// QWidget* MonitorWindow::makeCard(const QString& title, QLabel* valueLabel) {
//     QFrame* card = new QFrame;
//     card->setFrameShape(QFrame::StyledPanel);
//     card->setStyleSheet(R"(
//         QFrame { background: #ffffff; border-radius: 8px; }
//     )");
//     auto* v = new QVBoxLayout(card);
//     auto* t = new QLabel(title, card);
//     t->setStyleSheet("color:#555555; font-size:14px;");
//     valueLabel->setStyleSheet("font-size:24px; font-weight:bold;");
//     v->addWidget(t, 0, Qt::AlignLeft);
//     v->addWidget(valueLabel, 0, Qt::AlignCenter);
//     return card;
// }

QFrame* MonitorWindow::createEventCard(const QString& title, QLabel* value, const QString& borderColor, const QString& numberColor) {
    QFrame* frame = new QFrame;
    frame->setFixedSize(90, 85);
    frame->setStyleSheet(QString(
                             "QFrame {"
                             " border: none;"
                             " border-radius: 8px;"
                             " background-color: %1;"
                             " }"
                             ).arg(borderColor));

    QVBoxLayout* layout = new QVBoxLayout(frame);
    layout->setSpacing(4);
    layout->setContentsMargins(6, 8, 6, 8); // 상하 여백 조정
    layout->setAlignment(Qt::AlignCenter);

    value->setStyleSheet(QString("color: %1; font-size: 20px; font-weight: bold; background-color: transparent;").arg(numberColor));
    value->setAlignment(Qt::AlignCenter);

    QLabel* titleLabel = new QLabel(title);
    titleLabel->setStyleSheet(QString(
                                  "color: #FFFFFF;"
                                  "font-size: 12px;"
                                  "font-weight: normal;"
                                  "background-color: transparent;"
        ));
    titleLabel->setAlignment(Qt::AlignCenter);

    layout->addWidget(value);
    layout->addWidget(titleLabel);

    return frame;
}

QWidget* MonitorWindow::createPeopleLabel(const QString& labelText, QLabel* countLabel, const QColor& color) {
    QWidget* row = new QWidget;
    QHBoxLayout* layout = new QHBoxLayout(row);
    layout->setContentsMargins(2, 2, 2, 2);
    layout->setSpacing(10);

    QLabel* gateLabel = new QLabel(labelText);
    gateLabel->setStyleSheet("color: #ffffff; font-size: 12px;");
    layout->addWidget(gateLabel);

    countLabel->setStyleSheet("color: #33ff99; font-size: 13px; font-weight: bold;");
    layout->addStretch();
    layout->addWidget(countLabel);

    QLabel* statusDot = new QLabel;
    statusDot->setFixedSize(10, 10);
    statusDot->setStyleSheet(QString("background-color: %1; border-radius: 5px;").arg(color.name()));
    layout->addWidget(statusDot);

    return row;
}

QGroupBox* MonitorWindow::createPeopleBox(const QString& title, QLabel* countLabel, const QColor& color) {
    QGroupBox* box = new QGroupBox;
    box->setStyleSheet(QString(
                           "QGroupBox { border: 1px solid %1; border-radius: 6px; background-color: #141B2B; padding: 4px; }"
                           "QLabel { color: white; }"
                           ).arg(color.name()));

    QVBoxLayout* layout = new QVBoxLayout(box);
    layout->setSpacing(3);
    layout->setContentsMargins(4, 4, 4, 4);

    countLabel->setStyleSheet(QString("font-size: 20px; color: %1; font-weight: bold;").arg(color.name()));
    countLabel->setAlignment(Qt::AlignCenter);

    QLabel* titleLabel = new QLabel(title);
    titleLabel->setStyleSheet("font-size: 12px; color: #cccccc;");
    titleLabel->setAlignment(Qt::AlignCenter);

    layout->addWidget(countLabel);
    layout->addWidget(titleLabel);

    return box;
}

void MonitorWindow::showEventDetailsPopup(const QString& eventTime) {
    QSqlQuery q;
    q.prepare("SELECT image_path, json_data FROM fall_events WHERE user_id = ? AND event_time = ?");
    q.addBindValue(m_currentUserId);
    q.addBindValue(eventTime);

    if (q.exec() && q.next()) {
        QString imagePath = q.value(0).toString();
        QString json = q.value(1).toString();

        // JSON 파싱
        QJsonDocument doc = QJsonDocument::fromJson(json.toUtf8());
        QJsonObject obj = doc.object();

        // QString indoorText = "-";
        // QString gateTexts[3] = { "-", "-", "-" };

        // // 실내 총 인원
        // if (obj.contains("indoor_people_count")) {
        //     QJsonValue val = obj["indoor_people_count"];
        //     if (val.isDouble()) {
        //         int indoorCount = static_cast<int>(val.toDouble());
        //         indoorText = QString("%1 실내 총 인원").arg(indoorCount);
        //     }
        // }

        int indoorCount = -1;
        int gatePeople[3] = {0, 0, 0};
        int gateDists[3] = {0, 0, 0};
        double gateScores[3] = {0.0, 0.0, 0.0};
        int bestIdx = -1;
        // 🔍 실내 인원
        if (obj.contains("indoor_people_count") && obj["indoor_people_count"].isDouble()) {
            indoorCount = static_cast<int>(obj["indoor_people_count"].toDouble());
        }
        for (int i = 1; i <= 3; ++i) {
            QString distKey = QString("gate_%1_dist").arg(i);
            QString countKey = QString("gate_%1_people_count").arg(i);
            QString scoreKey = QString("gate_%1_score").arg(i);

            if (obj.contains(distKey) && obj.contains(countKey) && obj.contains(scoreKey)) {
                gateDists[i - 1] = static_cast<int>(obj[distKey].toDouble());
                gatePeople[i - 1] = static_cast<int>(obj[countKey].toDouble());
                gateScores[i - 1] = obj[scoreKey].toDouble();
            }
        }
        // 🔍 최적 게이트 인덱스
        if (obj.contains("min_gate_idx") && obj["min_gate_idx"].isDouble()) {
            bestIdx = obj["min_gate_idx"].toInt();  // 1~3
        }

        // // 각 게이트 정보 파싱
        // for (int i = 1; i <= 3; ++i) {
        //     QString distKey = QString("gate_%1_dist").arg(i);
        //     QString countKey = QString("gate_%1_people_count").arg(i);
        //     QString scoreKey = QString("gate_%1_score").arg(i);

        //     if (obj.contains(distKey) && obj.contains(countKey) && obj.contains(scoreKey)) {
        //         int dist  = static_cast<int>(obj[distKey].toDouble());
        //         int people = static_cast<int>(obj[countKey].toDouble());
        //         double score = 10.0 - obj[scoreKey].toDouble();
        //         gateTexts[i - 1] = QString("게이트 %1: 거리 %2, 인원 %3명, 점수 %4/10")
        //                                .arg(i).arg(dist).arg(people).arg(QString::number(score, 'f', 2));
        //     }
        // }

        // 낙상 위치
        QString fallPosStr = "📍 낙상 위치: (-, -)";
        if (obj.contains("fall_point_x") && obj.contains("fall_point_y")) {
            int x = static_cast<int>(obj["fall_point_x"].toDouble());
            int y = static_cast<int>(obj["fall_point_y"].toDouble());
            fallPosStr = QString("📍 낙상 위치: (%1, %2)").arg(x).arg(y);
        }

        // 최적 게이트
        QString bestGateText = "최적 경로 정보 없음";

        if (obj.contains("min_gate_idx") && obj["min_gate_idx"].isDouble()) {
            int bestIdx = obj["min_gate_idx"].toInt();
            QString distKey = QString("gate_%1_dist").arg(bestIdx);
            QString scoreKey = QString("gate_%1_score").arg(bestIdx);
            QString countKey = QString("gate_%1_people_count").arg(bestIdx);

            if (obj.contains(distKey) && obj.contains(scoreKey) && obj.contains(countKey)) {
                int dist = static_cast<int>(obj[distKey].toDouble());
                double score = 10.0 - obj[scoreKey].toDouble();
                int people = static_cast<int>(obj[countKey].toDouble());

                bestGateText = QString("✅ 게이트 %1\n거리: %2m\n점수: %3/10 \n대기인원: %4명")
                                   .arg(bestIdx)
                                   .arg(dist)
                                   .arg(QString::number(score, 'f', 2))
                                   .arg(people);
            }
        }

        // 팝업 구성
        QDialog* dialog = new QDialog(this);
        dialog->setWindowTitle("📋 이벤트 상세 정보");
        dialog->setStyleSheet("background-color: #1e2938; color: white;");

        QVBoxLayout* mainLayout = new QVBoxLayout(dialog);

        QString groupBoxStyle = R"(
            QGroupBox {
                background-color: #1e2938;
                border: 1px solid #394b5e;
                border-radius: 10px;
                padding: 10px;
                margin-top: 10px;
                color: white;
                font-size: 13px;
            }
            QGroupBox::title {
                subcontrol-origin: margin;
                subcontrol-position: top left;
                padding: 0 5px;
                font-weight: bold;
                font-size: 14px;
            }
        )";

        // ✅ 왼쪽 박스: 이벤트 발생 사진 + 기본 정보
        QHBoxLayout* leftLayout = new QHBoxLayout;

        // 1. 이벤트 발생 사진
        QGroupBox* photoBox = new QGroupBox("🖼️ 이벤트 발생 사진");
        photoBox->setStyleSheet(groupBoxStyle);
        QVBoxLayout* photoLayout = new QVBoxLayout(photoBox);
        QLabel* photo = new QLabel;
        photo->setPixmap(QPixmap(imagePath).scaled(320, 240, Qt::KeepAspectRatio));
        photoLayout->addWidget(photo);
        leftLayout->addWidget(photoBox);

        // 2. 기본 정보
        QGroupBox* infoBox = new QGroupBox("📋 기본 정보");
        QVBoxLayout* infoLayout = new QVBoxLayout(infoBox);
        infoBox->setStyleSheet(groupBoxStyle);

        QLabel* fallLabel = new QLabel("🟥 낙상감지    🟧 진행중");
        fallLabel->setStyleSheet("font-weight: bold; font-size: 13px;");
        QLabel* timeLabel = new QLabel("🕒 시간: " + QDateTime::fromString(eventTime, "yyyyMMdd_hhmmss").toString("yyyy-MM-dd hh:mm:ss"));
        QLabel* camLabel = new QLabel("📷 감지 카메라: 실내 카메라");
        QLabel* posLabel = new QLabel(fallPosStr);
        QLabel* descLabel = new QLabel("📄 상세 설명:\n중앙에서 낙상이 감지되었습니다.\n즉시 응급 대응이 필요합니다.");
        descLabel->setWordWrap(true);

        infoLayout->addWidget(fallLabel);
        infoLayout->addWidget(timeLabel);
        infoLayout->addWidget(camLabel);
        infoLayout->addWidget(posLabel);
        infoLayout->addWidget(descLabel);

        leftLayout->addWidget(infoBox);
        mainLayout->addLayout(leftLayout);

        // ✅ 오른쪽 박스: 군중 분석 정보 + 최적 대피 경로
        QHBoxLayout* rightLayout = new QHBoxLayout;

        // 3. 군중 분석 데이터
        QGroupBox* analysisBox = new QGroupBox("👥 군중 분석 데이터");
        analysisBox->setStyleSheet(groupBoxStyle);
        QVBoxLayout* analysisLayout = new QVBoxLayout(analysisBox);

        // 1. 실내 총 인원 박스
        QVBoxLayout* indoorCountLayout = new QVBoxLayout;
        QLabel* indoorNumber = new QLabel(QString::number(indoorCount));
        indoorNumber->setStyleSheet(R"(
    font-size: 22px;
    font-weight: bold;
    color: white;
    padding-bottom: 4px;
)");
        indoorNumber->setAlignment(Qt::AlignCenter);

        QLabel* indoorText = new QLabel("실내 총 인원");
        indoorText->setStyleSheet("font-size: 12px; color: white;");
        indoorText->setAlignment(Qt::AlignCenter);

        indoorCountLayout->addWidget(indoorNumber);
        indoorCountLayout->addWidget(indoorText);

        QWidget* indoorCountBox = new QWidget;
        indoorCountBox->setLayout(indoorCountLayout);
        indoorCountBox->setStyleSheet(R"(
    background-color: #1e3a8a;
    border-radius: 8px;
    padding: 8px;
)");
        analysisLayout->addWidget(indoorCountBox);


        // 2. 게이트별 카드 생성 함수
        auto createGateCard = [](int gateNum, int people, int dist, double score, bool isBest) -> QWidget* {
            QString badge = isBest ? "최적 경로" : "일반";
            QString badgeStyle = isBest
                                     ? "background-color: #166534; color: white; padding: 2px 6px; border-radius: 4px; font-size: 11px;"
                                     : "background-color: #4b5563; color: white; padding: 2px 6px; border-radius: 4px; font-size: 11px;";

            QLabel* topLine = new QLabel(QString("게이트 %1").arg(gateNum));
            topLine->setStyleSheet("font-weight: bold; color: white;");
            QLabel* badgeLabel = new QLabel(badge);
            badgeLabel->setStyleSheet(badgeStyle);

            QHBoxLayout* topLayout = new QHBoxLayout;
            topLayout->addWidget(topLine);
            topLayout->addStretch();
            topLayout->addWidget(badgeLabel);

            auto makeLabeledValue = [](const QString& label, const QString& value, const QString& color) -> QWidget* {
                QLabel* labelWidget = new QLabel(label);
                labelWidget->setStyleSheet(QString("font-size: 11px; color: %1;").arg(color));
                QLabel* valueWidget = new QLabel(value);
                valueWidget->setStyleSheet("font-size: 13px; color: white; font-weight: bold;");
                labelWidget->setAlignment(Qt::AlignCenter);
                valueWidget->setAlignment(Qt::AlignCenter);

                QVBoxLayout* layout = new QVBoxLayout;
                layout->setContentsMargins(0, 0, 0, 0);
                layout->addWidget(labelWidget);
                layout->addWidget(valueWidget);

                QWidget* box = new QWidget;
                box->setLayout(layout);
                return box;
            };

            QWidget* peopleBox = makeLabeledValue("대기인원", QString("%1명").arg(people), "#60a5fa");   // 하늘색
            QWidget* distBox   = makeLabeledValue("거리",  QString("%1m").arg(dist),   "#c084fc");   // 보라색
            QWidget* score_Box  = makeLabeledValue("점수",  QString("%1/10").arg(QString::number(score, 'f', 2)), "#f97316"); // 주황색

            QHBoxLayout* infoLayout = new QHBoxLayout;
            infoLayout->addWidget(peopleBox);
            infoLayout->addWidget(distBox);
            infoLayout->addWidget(score_Box);

            QVBoxLayout* gateCardLayout = new QVBoxLayout;
            gateCardLayout->addLayout(topLayout);
            gateCardLayout->addSpacing(5);
            gateCardLayout->addLayout(infoLayout);

            QWidget* card = new QWidget;
            card->setLayout(gateCardLayout);
            card->setStyleSheet(R"(
        background-color: #334155;
        border: 1px solid #475569;
        border-radius: 8px;
        padding: 10px;
    )");

            return card;
        };

        // 3. 각 게이트 카드 추가
        for (int i = 0; i < 3; ++i) {
            bool isBest = (i + 1 == bestIdx);  // bestIdx는 json에서 가져온 최적 게이트 번호
            QWidget* gateCard = createGateCard(
                i + 1,
                gatePeople[i],  // 게이트별 인원
                gateDists[i],   // 게이트별 거리
                gateScores[i],  // 게이트별 점수
                isBest
                );
            analysisLayout->addWidget(gateCard);
        }
        rightLayout->addWidget(analysisBox);

        // 4. 최적 대피 경로
        QVBoxLayout* fourLayout = new QVBoxLayout;
        QGroupBox* optimalBox = new QGroupBox("🛣 최적 대피 경로");
        optimalBox->setStyleSheet(groupBoxStyle);
        QVBoxLayout* optimalLayout = new QVBoxLayout(optimalBox);
        optimalLayout->setSpacing(4);  // 줄 간격 줄임
        optimalLayout->setContentsMargins(6, 6, 6, 6);

        QLabel* best = new QLabel(bestGateText);
        best->setStyleSheet(R"(
            background-color: #14532d;
            border: 1px solid #22c55e;
            color: white;
            font-weight: bold;
            padding: 8px;
            border-radius: 8px;
        )");
        QLabel* warn = new QLabel("⚠ 이 경로는 거리, 현재 인원수, 게이트 점수를 종합하여 서버로부터 추천된 대피 경로입니다.");
        warn->setWordWrap(true);
        warn->setStyleSheet("color: #facc15; font-size: 11px; line-height: 1.2;");

        optimalLayout->addWidget(best);
        optimalLayout->addWidget(warn);

        optimalBox->setFixedHeight(200);
        fourLayout->addWidget(optimalBox);
        fourLayout->addStretch();
        rightLayout->addLayout(fourLayout);
        mainLayout->addLayout(rightLayout);

        // ✅ 하단 버튼
        // QPushButton* downloadBtn = new QPushButton("📥 리포트 다운로드");
        QPushButton* closeBtn = new QPushButton("닫기");
        closeBtn->setCursor(Qt::PointingHandCursor);
        closeBtn->setStyleSheet(R"(
            QPushButton {
                background-color: #2563eb;    /* 진한 파랑 */
                color: white;
                padding: 8px 16px;
                border: none;
                border-radius: 6px;
                font-weight: bold;
            }
            QPushButton:hover {
                background-color: #3b82f6;    /* 밝은 파랑 */
            }
        )");
        connect(closeBtn, &QPushButton::clicked, dialog, &QDialog::close);

        QHBoxLayout* btnLayout = new QHBoxLayout;
        btnLayout->addStretch();
        // btnLayout->addWidget(downloadBtn);
        btnLayout->addWidget(closeBtn);

        mainLayout->addLayout(btnLayout);
        dialog->resize(500, 800);
        dialog->exec();
    } else {
        qWarning() << "[EventDetails] 조회 실패:" << q.lastError().text();
    }
}


bool MonitorWindow::eventFilter(QObject* watched, QEvent* event) {
    if (event->type() == QEvent::MouseButtonDblClick) {
        QWidget* widget = qobject_cast<QWidget*>(watched);
        if (widget && m_cardEventTimes.contains(widget)) {
            QString eventTime = m_cardEventTimes[widget];
            showEventDetailsPopup(eventTime);  // 팝업 띄우기
            return true;
        }
    }
    return QWidget::eventFilter(watched, event);
}


QWidget* MonitorWindow::createDashboardWidget() {
    QWidget* dash = new QWidget;
    dash->setStyleSheet("background-color: #1e2939; color: white;");

    auto* DashVbox = new QVBoxLayout(dash);
    DashVbox->setContentsMargins(2,2,2,2);
    DashVbox->setSpacing(8);

    // ── 1) Today's event status ───────────────────────────────────────────
    auto* TodayEventBox = new QGroupBox(dash);
    TodayEventBox->setTitle("");
    TodayEventBox->setStyleSheet("QGroupBox { border: 1px solid #444; border-radius: 8px; margin-top: 10px; padding: 6px; }");
    auto* TodayEventLayout = new QVBoxLayout(TodayEventBox);
    TodayEventLayout->setContentsMargins(6, 6, 6, 6);
    TodayEventLayout->setSpacing(2);

    // Title
    QLabel *iconTodayEventLabel = new QLabel;
    iconTodayEventLabel->setPixmap(QPixmap(":/new/prefix1/images/Today_Event.png").scaled(32, 32, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    QLabel *titleTodayEventLabel = new QLabel("Today's event status");
    titleTodayEventLabel->setStyleSheet("color: white; font-size: 20px; font-weight: bold;");
    QHBoxLayout *TodayEventTitleLayout = new QHBoxLayout;
    TodayEventTitleLayout->addWidget(iconTodayEventLabel);
    TodayEventTitleLayout->addWidget(titleTodayEventLabel);
    TodayEventTitleLayout->addStretch();
    TodayEventLayout->addLayout(TodayEventTitleLayout);

    QHBoxLayout* TELayout = new QHBoxLayout();
    TELayout->setSpacing(10);
    TELayout->setContentsMargins(10, 10, 10, 10);
    TELayout->setAlignment(Qt::AlignCenter);

    totalEvents = todayFallCount + todayOvercrowded;
    QLabel* TotalLabel = new QLabel(QString::number(totalEvents));
    QFrame* totalEvent = createEventCard("총 이벤트", TotalLabel, "#352f33", "#ff8904");

    // 낙상감지
    todayFallCount = getTodayFallEventCount();
    QLabel* countLabel = new QLabel(QString::number(todayFallCount));
    QFrame* fallDetect = createEventCard("낙상감지", countLabel, "#342938", "#ff6467");

    // QLabel* OverCrowdLabel = new QLabel(QString::number(0));
    todayOvercrowded = getTodayOvercrowdedCount();
    QLabel* OverCrowdLabel = new QLabel(QString::number(todayOvercrowded));
    QTimer* fallCardTimer = new QTimer(this);
    connect(fallCardTimer, &QTimer::timeout, this, [=]() {
        if (!m_currentUserId.isEmpty()) {
            int countFall = getTodayFallEventCount();
            int countOver = getTodayOvercrowdedCount();
            todayFallCount = countFall + countOver;

            countLabel->setText(QString::number(countFall));
            OverCrowdLabel->setText(QString::number(countOver));
            TotalLabel->setText(QString::number(todayFallCount));  // ✅ 총합 갱신
        }
    });
    fallCardTimer->start(1000);
    QFrame* overcrowded = createEventCard("과밀집", OverCrowdLabel, "#333733", "#fdc700");
    TELayout->setSpacing(2);
    TELayout->addWidget(totalEvent);
    TELayout->setSpacing(1);
    TELayout->addWidget(fallDetect);
    TELayout->setSpacing(1);
    TELayout->addWidget(overcrowded);
    TELayout->setSpacing(2);
    TodayEventLayout->addLayout(TELayout, 1);

    DashVbox->addWidget(TodayEventBox, /*stretch=*/1);

    // // ✅ 쓰러진 사람 여부 확인
    // m_activeAlarmLabel = new QLabel("✅ No Fall Detected");
    // m_activeAlarmLabel->setStyleSheet("color: green; font-size: 18px; font-weight: bold;");
    // grid->addWidget(makeCard("Active Alarms", m_activeAlarmLabel), 0, 1);

    // // ✅ gate 좌표
    // m_cameraCoordLabel = new QLabel("좌표 불러오는 중...");
    // m_cameraCoordLabel->setAlignment(Qt::AlignCenter);  // 가운데 정렬 (선택사항)
    // m_cameraCoordLabel->setStyleSheet("font-family: Consolas;"); // 고정폭 폰트 (선택사항)
    // grid->addWidget(makeCard("Camera Coordinates", m_cameraCoordLabel), 0, 2);

    // ── 2) Real-time status ─────────────────────────────────
    auto* RealTimeBox = new QGroupBox(dash);
    RealTimeBox->setTitle("");
    RealTimeBox->setStyleSheet("QGroupBox { border: 1px solid #444; border-radius: 8px; margin-top: 10px; padding: 6px; }");
    auto* RealTimeLayout = new QVBoxLayout(RealTimeBox);
    RealTimeLayout->setContentsMargins(6, 6, 6, 6);
    RealTimeLayout->setSpacing(2);

    // DataManagement Title
    QLabel *iconRealTimeLabel = new QLabel;
    QLabel *titleRealTimeLabel = new QLabel("Real-time status");
    titleRealTimeLabel->setStyleSheet("color: white; font-size: 20px; font-weight: bold;");
    iconRealTimeLabel->setPixmap(QPixmap(":/new/prefix1/images/Real_Time.png").scaled(32, 32, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    QHBoxLayout *RealTimeTitleLayout = new QHBoxLayout;
    RealTimeTitleLayout->addWidget(iconRealTimeLabel);
    RealTimeTitleLayout->addWidget(titleRealTimeLabel);
    RealTimeTitleLayout->addStretch();
    RealTimeLayout->addLayout(RealTimeTitleLayout);

    // Indoor Density
    QHBoxLayout* titleLayout = new QHBoxLayout;
    QLabel* titleLabel = new QLabel("Indoor Density");
    percentLabel = new QLabel(QString("%1%").arg(static_cast<double>(crowdPercent), 0, 'f', 1));
    titleLabel->setStyleSheet("color: #ffffff; font-size: 13px; font-weight: normal; background-color: transparent;");
    percentLabel->setStyleSheet("color: #ff5249; font-size: 16px; font-weight: bold;");
    titleLayout->addWidget(titleLabel);
    titleLayout->addStretch();
    titleLayout->addWidget(percentLabel);
    RealTimeLayout->addLayout(titleLayout);
    // Progress Bar
    m_crowdProgressBar = new QProgressBar;
    m_crowdProgressBar->setTextVisible(false);
    m_crowdProgressBar->setFixedHeight(10);
    m_crowdProgressBar->setStyleSheet(R"(
        QProgressBar {
            border: none;
            border-radius: 6px;
            background-color: #192132;
        }
        QProgressBar::chunk {
            background-color: #030213;
            border-radius: 6px;
        }
    )");
    m_crowdProgressBar->setRange(0, 100);
    m_crowdProgressBar->setValue(crowdPercent);  // 초기값
    RealTimeLayout->addWidget(m_crowdProgressBar);
    // Warning Text
    QLabel* warning = new QLabel("⚠️ 실내 밀집도 80% 초과 시 경고 알림");
    warning->setStyleSheet("color: #ff6467; font-size: 13px; font-weight: bold;");
    RealTimeLayout->addWidget(warning);
    // Personnel Box
    QHBoxLayout* countLayout = new QHBoxLayout;
    indoorCountLabel = new QLabel("0");
    QGroupBox* indoorBox = createPeopleBox("NO. people indoor", indoorCountLabel, QColor("#3399ff"));
    indoorBox->findChild<QLabel*>()->setText("0"); // or skip if using label directly
    countLayout->addWidget(indoorBox);
    gateCountLabel = new QLabel("0");
    QGroupBox* gateBox = createPeopleBox("NO. people Gate", gateCountLabel, QColor("#33ff99"));
    countLayout->addWidget(gateBox);
    RealTimeLayout->addLayout(countLayout);
    // Number of people per gate
    QVBoxLayout* gateLayout = new QVBoxLayout;
    gate1CountLabel = new QLabel("0명");
    gate2CountLabel = new QLabel("0명");
    gate3CountLabel = new QLabel("0명");
    gateLayout->addWidget(createPeopleLabel("Gate 1", gate1CountLabel, Qt::black));
    gateLayout->addWidget(createPeopleLabel("Gate 2", gate2CountLabel, Qt::black));
    gateLayout->addWidget(createPeopleLabel("Gate 3", gate3CountLabel, Qt::black));
    RealTimeLayout->addLayout(gateLayout);

    DashVbox->addWidget(RealTimeBox, /*stretch=*/2);

    // ── 3) Event Log ─────────────────────────────────
    auto* EventLogBox = new QGroupBox(dash);
    EventLogBox->setTitle("");
    EventLogBox->setStyleSheet("QGroupBox { border: 1px solid #444; border-radius: 8px; margin-top: 10px; padding: 6px; }");
    auto* EventLogLayout = new QVBoxLayout(EventLogBox);
    EventLogLayout->setContentsMargins(6, 6, 6, 6);
    EventLogLayout->setSpacing(2);

    // DataManagement Title
    QLabel *iconEventLogLabel = new QLabel;
    QLabel *titleEventLogLabel = new QLabel("Event Log");
    titleEventLogLabel->setStyleSheet("color: white; font-size: 20px; font-weight: bold;");
    iconEventLogLabel->setPixmap(QPixmap(":/new/prefix1/images/Event_Log.png").scaled(32, 32, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    QHBoxLayout *EventLogTitleLayout = new QHBoxLayout;
    EventLogTitleLayout->addWidget(iconEventLogLabel);
    EventLogTitleLayout->addWidget(titleEventLogLabel);
    EventLogTitleLayout->addStretch();
    EventLogLayout->addLayout(EventLogTitleLayout);

    // 이벤트 기록
    QScrollArea* scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    scroll->setStyleSheet("QScrollArea { background-color: #141622; border: none; }");

    QWidget* scrollWidget = new QWidget;
    scrollLayout = new QVBoxLayout(scrollWidget);
    scrollLayout->setContentsMargins(0, 0, 0, 0);
    scrollLayout->setSpacing(8);  // 카드 간 간격

    scroll->setWidget(scrollWidget);
    EventLogLayout->addWidget(scroll);

    // DB에서 이벤트 로딩
    QSqlQuery query("SELECT event_time, image_path, json_Data FROM fall_events WHERE user_id = ? ORDER BY event_time DESC");
    query.addBindValue(m_currentUserId);
    if (!query.exec()) {
        qWarning() << "[FallEventTab] 쿼리 실패:" << query.lastError().text();
    } else {
        while (query.next()) {
            QString eventTime = query.value(0).toString();
            QString imagePath = query.value(1).toString();
            QString jsonData = query.value(2).toString();

            // 카드 위젯 생성
            QFrame* card = new QFrame;
            card->setObjectName("eventCard");
            card->setStyleSheet(R"(
                #eventCard {
                    background-color: #2a3545;
                    border: 1px solid #4a5565;
                    border-radius: 8px;
                }
            )");
            QVBoxLayout* cardLayout = new QVBoxLayout(card);
            cardLayout->setContentsMargins(10, 8, 10, 8);
            cardLayout->setSpacing(4);

            // 상태 뱃지 (낙상감지 / 진행중 or 해결됨)
            QLabel* statusLabel = new QLabel("🔺 Fall Detection");
            QLabel* resolvedLabel = new QLabel("🟢 Solved");  // 조건에 따라 "진행중"으로 바꿔도 됨
            statusLabel->setStyleSheet("background-color: #543342; color: #ff6467; border-radius: 4px; padding: 2px 6px;");
            statusLabel->setStyleSheet(R"(
                QLabel {
                    background-color: #543342;
                    color: #ff6467;
                    border: 1px solid #ff6467;
                    border-radius: 6px;
                    padding: 2px 6px;
                })");
            resolvedLabel->setStyleSheet(R"(
                QLabel {
                    background-color: #27ae60;
                    color: white;
                    border: 1px solid white;
                    border-radius: 6px;
                    padding: 2px 6px;
                })");

            QHBoxLayout* statusLayout = new QHBoxLayout;
            statusLayout->addWidget(statusLabel);
            statusLayout->addWidget(resolvedLabel);
            statusLayout->addStretch();

            // 타이틀 + 경로
            QString cameraText = "Camera: Indoor";  // 기본값
            QString pathText = "Optimal Route: -";  // 기본값

            QJsonParseError err;
            QJsonDocument doc = QJsonDocument::fromJson(jsonData.toUtf8(), &err);
            if (err.error == QJsonParseError::NoError && doc.isObject()) {
                QJsonObject obj = doc.object();

                // ✅ min_gate_idx 기반 최적 경로 문자열 생성
                if (obj.contains("min_gate_idx") && obj["min_gate_idx"].isDouble()) {
                    int gateIdx = obj["min_gate_idx"].toInt();
                    if (gateIdx >= 1 && gateIdx <= 3){
                        pathText = QString("최적 경로: Gate %1").arg(gateIdx);
                    }
                }
            }
            QLabel* cameraLabel = new QLabel(cameraText);
            QLabel* pathLabel = new QLabel(pathText);
            cameraLabel->setStyleSheet("background-color: #2a3545; color: white; font-weight: bold;");
            pathLabel->setStyleSheet("background-color: #2a3545; color: #51a2e1;");

            // 시간
            QLabel* timeLabel = new QLabel(QDateTime::fromString(eventTime, "yyyyMMdd_hhmmss").toString("yyyy-MM-dd hh:mm:ss"));
            timeLabel->setStyleSheet("background-color: #2a3545; color: #aaa; font-size: 11px;");

            // 구성
            cardLayout->addLayout(statusLayout);
            cardLayout->addWidget(cameraLabel);
            cardLayout->addWidget(pathLabel);
            cardLayout->addWidget(timeLabel);

            scrollLayout->addWidget(card);

            card->installEventFilter(this);  // 👈 이벤트 필터 등록
            m_cardEventTimes[card] = eventTime;  // 이벤트 시간 저장
        }
    }

    DashVbox->addWidget(EventLogBox, /*stretch=*/2);


    return dash;
}

void MonitorWindow::showCustomFallAlert(const QString& eventTime, const QString& imagePath, const QString& jsonData) {
    QDialog* dialog = new QDialog(this);
    dialog->setWindowTitle("🚨 구조 요청");
    dialog->setStyleSheet("background-color: #891f24; color: white; font-size: 14px;");

    QVBoxLayout* layout = new QVBoxLayout(dialog);

    // 🚨 구조 요청 문구
    QLabel* titleLabel = new QLabel("🚨 구조 요청\n낙상 이벤트가 감지되었습니다. 구조가 필요합니다.");
    titleLabel->setStyleSheet("font-size: 20px; font-weight: bold; color: white;");
    titleLabel->setAlignment(Qt::AlignCenter);
    layout->addWidget(titleLabel);

    // 위치 및 시각
    QLabel* locationLabel = new QLabel("위치: 실내 카메라");
    QLabel* timeLabel = new QLabel("시각: " + QDateTime::fromString(eventTime, "yyyyMMdd_hhmmss").toString("yyyy-MM-dd hh:mm:ss"));
    locationLabel->setStyleSheet("font-size: 13px;");
    timeLabel->setStyleSheet("font-size: 13px;");
    layout->addWidget(locationLabel);
    layout->addWidget(timeLabel);

    // 이미지
    QLabel* imageLabel = new QLabel;
    QPixmap image(imagePath);
    imageLabel->setPixmap(image.scaled(500, 280, Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation));
    imageLabel->setStyleSheet("border-radius: 10px;");
    layout->addWidget(imageLabel);

    // JSON 파싱
    QJsonDocument doc = QJsonDocument::fromJson(jsonData.toUtf8());
    QJsonObject obj = doc.object();
    int gateIdx = obj["min_gate_idx"].toInt();
    double score = 10.0 - obj[QString("gate_%1_score").arg(gateIdx)].toDouble();

    QLabel* gateLabel = new QLabel("최적 대피 경로");
    QLabel* routeLabel = new QLabel(QString("게이트 %1").arg(gateIdx));
    QLabel* scoreLabel = new QLabel(QString("점수: %1").arg(QString::number(score, 'f', 1)));

    gateLabel->setStyleSheet("font-size: 13px; margin-top: 10px;");
    routeLabel->setStyleSheet("font-size: 20px; font-weight: bold; color: #facc15;");
    scoreLabel->setStyleSheet("font-size: 13px; color: #f3f4f6;");

    layout->addWidget(gateLabel);
    layout->addWidget(routeLabel);
    layout->addWidget(scoreLabel);

    // 종료 버튼
    QPushButton* closeBtn = new QPushButton("닫기");
    closeBtn->setStyleSheet(R"(
        QPushButton {
            background-color: #16a34a;
            color: white;
            padding: 10px;
            font-weight: bold;
            border-radius: 6px;
        }
        QPushButton:hover {
            background-color: #22c55e;
        }
    )");
    connect(closeBtn, &QPushButton::clicked, dialog, &QDialog::accept);
    layout->addWidget(closeBtn);

    dialog->resize(520, 500);
    dialog->exec();
}


QWidget* MonitorWindow::createSettingTab() {
    QWidget* SettingDash = new QWidget;
    SettingDash->setStyleSheet(R"(
        QWidget {
            background-color: #1E2531;
            color: white;
        }
        QGroupBox {
            background-color: #1E2531;
            border: 1px solid #404B5C;
            border-radius: 18px;
            padding: 13px 10px;
            margin: 4px;
            font-weight: bold;
            font-size: 14px;
        }
        QGroupBox::title {
            subcontrol-origin: margin;
            subcontrol-position: top left;
            padding: 0 8px;
            color: #3498DB;
            font-weight: bold;
            font-size: 14px;
        }
        QLabel {
            color: #ECF0F1;
            font-size: 12px;
            padding: 2px;
            background-color: transparent;
        }
        QLineEdit {
            background-color: #2A3441;
            border: 1px solid #52637A;
            border-radius: 10px;
            padding: 8px 12px;
            color: white;
            font-size: 13px;
            min-height: 20px;
        }
        QLineEdit:focus {
            border: 2px solid #4A9EFF;
            background-color: #2F3A4B;
        }
        QPushButton {
            background-color: #3498DB;
            color: white;
            border: none;
            border-radius: 6px;
            padding: 8px 16px;
            font-size: 12px;
            font-weight: bold;
            min-height: 20px;
        }
        QPushButton:hover {
            background-color: #2980B9;
        }
        QPushButton:pressed {
            background-color: #21618C;
        }
        QFrame[frameShape="4"] {
            background-color: #404B5C;
            border: none;
            max-height: 1px;
            margin: 6px 0;
        }
    )");
    auto* SettingVbox = new QVBoxLayout(SettingDash);
    SettingVbox->setContentsMargins(8, 8, 8, 8);
    SettingVbox->setSpacing(8);

    // ── 1) Indoor density warning threshold ───────────────────────────────────────────
    auto* AlertSettingBox = new QGroupBox("");
    auto* AlertLayout = new QVBoxLayout(AlertSettingBox);

    // 제목에 아이콘 추가
    QWidget* titleWidget = new QWidget;
    QHBoxLayout* titleLayout = new QHBoxLayout(titleWidget);
    titleLayout->setContentsMargins(0, 0, 0, 0);
    titleLayout->setSpacing(8);

    // Title
    QLabel *iconSettingLabel = new QLabel;
    QLabel *titleSettingLabel = new QLabel("Threshold settings");
    titleSettingLabel->setStyleSheet("color: white; font-size: 14px; font-weight: bold;");
    iconSettingLabel->setPixmap(QPixmap(":/new/prefix1/images/Setting.png").scaled(22, 22, Qt::KeepAspectRatio, Qt::SmoothTransformation));

    titleLayout->addWidget(iconSettingLabel);
    titleLayout->addWidget(titleSettingLabel);
    titleLayout->addStretch();

    AlertLayout->addWidget(titleWidget);
    AlertLayout->setContentsMargins(10, 10, 10, 8);
    AlertLayout->setSpacing(6);

    // 실내 밀집도 경고 임계값
    QLabel* indoorLabel = new QLabel("Indoor density warning threshold (%)");
    QLineEdit* indoorInput = new QLineEdit("80");
    indoorLabel->setStyleSheet("font-weight: normal; color: #ECF0F1; margin-bottom: 2px;");
    AlertLayout->addWidget(indoorLabel);
    AlertLayout->addWidget(indoorInput);
    // 구분선
    QFrame* line1 = new QFrame;
    line1->setFrameShape(QFrame::HLine);
    AlertLayout->addWidget(line1);

    // 게이트 혼잡도 임계값
    QLabel* gateTitle = new QLabel("Gate congestion threshold");
    gateTitle->setStyleSheet("font-weight: normal; color: #ECF0F1; margin-bottom: 4px;");
    AlertLayout->addWidget(gateTitle);
    // 경고 임계값
    QLabel* yellowLabel = new QLabel("경고 임계값 (노랑)");
    yellowLabel->setStyleSheet("color: #F39C12;");
    QLineEdit* yellowInput = new QLineEdit("15");
    AlertLayout->addWidget(yellowLabel);
    AlertLayout->addWidget(yellowInput);
    // 위험 임계값
    QLabel* redLabel = new QLabel("위험 임계값 (빨강)");
    redLabel->setStyleSheet("color: #E74C3C;");
    QLineEdit* redInput = new QLineEdit("30");
    AlertLayout->addWidget(redLabel);
    AlertLayout->addWidget(redInput);
    // 범례
    QLabel* legend = new QLabel("Re-login required when setting gate congestion threshold");
    legend->setStyleSheet("color: #BDC3C7; font-size: 11px; margin-top: 4px;");
    AlertLayout->addWidget(legend);

    SettingVbox->addWidget(AlertSettingBox);

    // return SettingDash;
    // ── 2) 게이트 좌표 설정 ───────────────────────────────────────────
    auto* GateBox = new QGroupBox("🔵 Gate coordinate settings");
    auto* GateLayout = new QVBoxLayout(GateBox);
    GateLayout->setContentsMargins(10, 13, 10, 13);
    GateLayout->setSpacing(8);

    // 게이트 UI 생성 함수 - 한 줄로 배치
    auto createGateWidget = [](const QString& gateName, const QString& dotColor, int defaultX, int defaultY) -> QWidget* {
        QWidget* container = new QWidget;
        QHBoxLayout* mainLayout = new QHBoxLayout(container);
        mainLayout->setContentsMargins(0, 0, 0, 0);
        mainLayout->setSpacing(8);

        // 게이트 도트
        QLabel* dot = new QLabel("●");
        dot->setStyleSheet(QString("color: %1; font-size: 14px;").arg(dotColor));
        dot->setFixedWidth(20);

        // 게이트 이름
        QLabel* nameLabel = new QLabel(gateName);
        nameLabel->setStyleSheet("font-weight: bold; color: #ECF0F1; font-size: 13px;");
        nameLabel->setFixedWidth(60);

        // x 레이블과 입력
        QLabel* xLabel = new QLabel("x:");
        xLabel->setStyleSheet("color: #BDC3C7; font-size: 12px;");

        QLineEdit* xEdit = new QLineEdit(QString::number(defaultX));
        xEdit->setFixedWidth(50);
        xEdit->setStyleSheet(R"(
            QLineEdit {
                background-color: #3A4454;
                border: 1px solid #52637A;
                border-radius: 10px;
                padding: 4px 6px;
                color: white;
                font-size: 11px;
                min-height: 14px;
            }
            QLineEdit:focus {
                border: 2px solid #4A9EFF;
                background-color: #404B5C;
            }
        )");

        // y 레이블과 입력
        QLabel* yLabel = new QLabel("y:");
        yLabel->setStyleSheet("color: #BDC3C7; font-size: 12px;");

        QLineEdit* yEdit = new QLineEdit(QString::number(defaultY));
        yEdit->setFixedWidth(50);
        yEdit->setStyleSheet(R"(
            QLineEdit {
                background-color: #3A4454;
                border: 1px solid #52637A;
                border-radius: 10px;
                padding: 4px 6px;
                color: white;
                font-size: 11px;
                min-height: 14px;
            }
            QLineEdit:focus {
                border: 2px solid #4A9EFF;
                background-color: #404B5C;
            }
        )");

        mainLayout->addWidget(dot);
        mainLayout->addWidget(nameLabel);
        mainLayout->addWidget(xLabel);
        mainLayout->addWidget(xEdit);
        mainLayout->addWidget(yLabel);
        mainLayout->addWidget(yEdit);
        mainLayout->addStretch();

        return container;
    };


    // 게이트 1~3 위젯 생성 (이미지에 표시된 값들 사용)
    GateLayout->addWidget(createGateWidget("Gate 1", "#3498DB", 80, 250));
    GateLayout->addWidget(createGateWidget("Gate 2", "#3498DB", 280, 250));
    GateLayout->addWidget(createGateWidget("Gate 3", "#3498DB", 180, 250));

    SettingVbox->addWidget(GateBox);

    // ── 3) 사용 안내 ───────────────────────────────────────────
    auto* InstructionBox = new QGroupBox("💡 Instructions for use");
    auto* InstructionLayout = new QVBoxLayout(InstructionBox);
    InstructionLayout->setContentsMargins(10, 10, 10, 8);
    InstructionLayout->setSpacing(3);

    QStringList instructions = {
        "• Real-time camera monitoring",
        "• Crowd counting data tracking",
        "• Automatic alerts on fall detection",
        "• Optimal evacuation route calculation",
        "• LED display and voice guidance",
        "• Click event logs for detailed information",
        "• Real-time density check in map tab",
        "• Gate congestion color display"
    };

    for (const QString& instruction : instructions) {
        QLabel* instructionLabel = new QLabel(instruction);
        instructionLabel->setStyleSheet("color: #BDC3C7; font-size: 11px; margin: 1px 0;");
        InstructionLayout->addWidget(instructionLabel);
    }

    SettingVbox->addWidget(InstructionBox);

    // 하단에 설정 저장 버튼 추가
    QHBoxLayout* buttonLayout = new QHBoxLayout;
    buttonLayout->addStretch();

    QPushButton* saveButton = new QPushButton("Save Settings");
    saveButton->setFixedWidth(120);

    buttonLayout->addWidget(saveButton);

    SettingVbox->addLayout(buttonLayout);
    SettingVbox->addStretch();

    // 버튼 연결
    connect(saveButton, &QPushButton::clicked, this, [=]() {
        bool ok1 = false, ok2 = false;
            int yellow = yellowInput->text().toInt(&ok1);
            int red = redInput->text().toInt(&ok2);

            if (ok1 && ok2) {
                qDebug() << "[Setting] Yellow threshold:" << yellow;
                qDebug() << "[Setting] Red threshold:" << red;

                // 💡 TODO: 필요시 저장 로직 추가 (예: DB, 설정파일, 변수 등)
            } else {
                QMessageBox::warning(this, "입력 오류", "정수값만 입력해주세요.");
            }
        QMessageBox::information(this, "Settings Saved", "Settings have been saved successfully.");
    });

    return SettingDash;
}

QWidget* MonitorWindow::createDockWidgets() {
    QDockWidget* dock = new QDockWidget(tr("Dashboard / Settings"), this);

    cameraTabs = new QTabWidget(dock);
    cameraTabs->setStyleSheet(R"(
        QTabWidget::pane {
            border: none;
            background-color: #1B2533;
        }

        QTabBar {
            background-color: #1B2533;
        }

        QTabBar::tab {
            background: transparent;
            color: #999999;
            padding: 6px 18px;
            font-size: 13px;
            font-weight: 500;
            border: 1px solid transparent;
            border-radius: 12px;
            margin-right: 6px;
        }

        QTabBar::tab:selected {
            background-color: #ffffff;
            color: #1B2533;
            font-weight: bold;
            border: 1px solid #cccccc;
        }

        QTabBar::tab:hover {
            color: #dddddd;
        }
    )");
    // --- Dashboard 탭 생성 (플레이스홀더) ---
    cameraTabs->addTab(createDashboardWidget(), tr("Monitoring"));
    // Fall event 저장
    cameraTabs->addTab(createGraphTab(), tr("Graph"));

    // Main container widget
    QWidget* container = new QWidget(this);

    auto* vbox = new QVBoxLayout(container);
    vbox->setContentsMargins(2,2,2,2);
    vbox->setSpacing(5);

    auto* RecentDataBox = new QGroupBox(container);
    RecentDataBox->setTitle("");
    RecentDataBox->setStyleSheet(R"(
        QGroupBox {
            background-color: #1e2939;
            border: 1px solid #303544;
            border-radius: 10px;
            color: white;
            font-size: 13px;
            font-weight: bold;
            padding: 6px;
        }
    )");
    auto* RecentDataLayout = new QVBoxLayout(RecentDataBox);
    RecentDataLayout->setContentsMargins(2, 2, 2, 2);
    RecentDataLayout->setSpacing(2);

    // DataManagement Title
    QLabel *iconRecentDataLabel = new QLabel;
    iconRecentDataLabel->setPixmap(QPixmap(":/new/prefix1/images/Recent_Data.png").scaled(16, 16, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    iconRecentDataLabel->setStyleSheet("background-color: #1e2939;");
    QLabel *titleRecentDataLabel = new QLabel("Recent Data");
    titleRecentDataLabel->setStyleSheet("background-color: #1e2939; color: white; font-size: 14px; font-weight: bold;");
    QHBoxLayout *RecentDataTitleLayout = new QHBoxLayout;
    RecentDataTitleLayout->addWidget(iconRecentDataLabel);
    RecentDataTitleLayout->addWidget(titleRecentDataLabel);
    RecentDataTitleLayout->addStretch();
    RecentDataLayout->addLayout(RecentDataTitleLayout);

    // Alerts table - top section
    alertTable = new QTableWidget(0, 4, RecentDataBox);
    alertTable->setStyleSheet(R"(
        QTableWidget {
            background-color: #2a3545;
            color: white;
            font-size: 12px;
            gridline-color: #2B2F3A;
            border: none;
        }
        QHeaderView::section {
            background-color: #2d3c53;
            color: #BBBBBB;
            padding: 6px;
            border: none;
            font-weight: bold;
        }
        QTableCornerButton::section {
            background-color: #2d3c53;  /* 테이블 좌측 상단 빈칸 색상 */
            border: 1px solid #2a3545;
        }
        QTableWidget::item {
            padding: 6px;
        }
    )");
    alertTable->setHorizontalHeaderLabels({tr("Time"), tr("Camera"), tr("Event"), tr("Status")});
    alertTable->horizontalHeader()->setStretchLastSection(true);
    RecentDataLayout->addWidget(alertTable, /*stretch=*/1);
    vbox->addWidget(RecentDataBox, /*stretch=*/1);  // Smaller stretch factor

    // MQTT client - created on main thread with proper parent
    Mqtt* mqttClient = new Mqtt(this);  // Use MonitorWindow as parent instead of container
    mqttClient->hide();  // Hide MQTT log interface
    mqttClient->setMonitorWindow(this);

    // Log History - bottom section
    // auto* reservedBox = new QGroupBox(tr("Data Management"), container);
    auto* reservedBox = new QGroupBox(container);
    reservedBox->setTitle("");
    reservedBox->setStyleSheet(R"(
        QGroupBox {
            background-color: #1e2939;
            border: 1px solid #303544;
            border-radius: 10px;
            color: white;
            font-size: 13px;
            font-weight: bold;
            padding: 6px;
        }
    )");
    auto* reservedLayout = new QVBoxLayout(reservedBox);
    reservedLayout->setContentsMargins(2, 2, 2, 2);
    reservedLayout->setSpacing(2);

    // DataManagement Title
    QLabel *iconDataManagementLabel = new QLabel;
    iconDataManagementLabel->setPixmap(QPixmap(":/new/prefix1/images/Data_Management.png").scaled(16, 16, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    iconDataManagementLabel->setStyleSheet("background-color: #1e2939;");
    QLabel *titleDataManagementLabel = new QLabel("Data Management");
    titleDataManagementLabel->setStyleSheet("background-color: #1e2939; color: white; font-size: 14px; font-weight: bold;");
    // 수평 레이아웃에 아이콘 + 텍스트 넣기
    QHBoxLayout *dataManagementTitleLayout = new QHBoxLayout;
    dataManagementTitleLayout->addWidget(iconDataManagementLabel);
    dataManagementTitleLayout->addWidget(titleDataManagementLabel);
    dataManagementTitleLayout->addStretch();
    reservedLayout->addLayout(dataManagementTitleLayout);

    // ----- 필터 & 검색 입력 UI 추가 ----- //
    auto* filterLayout = new QHBoxLayout;
    // 필드 선택 콤보박스
    auto* filterCombo = new QComboBox(reservedBox);
    filterCombo->setStyleSheet("QComboBox { background-color: #2B2F3A; color: white; border-radius: 6px; padding: 4px; }");
    filterCombo->addItems({"Time", "Camera", "Event", "Status"});
    filterCombo->setMinimumWidth(100);
    // 입력 필드를 동적으로 전환
    auto* inputStack = new QStackedWidget(reservedBox);
    inputStack->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    inputStack->setFixedHeight(30);  // 또는 35 정도
    // 날짜 입력 위젯 (From ~ To)
    auto* dateWidget = new QWidget(reservedBox);
    dateWidget->setStyleSheet(R"(
        QWidget {
            background-color: #1e2939;
            border-radius: 6px;
        }
    )");
    auto* dateLayout = new QHBoxLayout(dateWidget);
    dateLayout->setContentsMargins(0, 0, 0, 0);
    dateLayout->setSpacing(6);  // 혹은 0도 OK
    dateWidget->setLayout(dateLayout);
    dateWidget->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);  // 높이 고정
    auto* fromDate = new QDateEdit(QDate::currentDate().addDays(-7), reservedBox);
    fromDate->setStyleSheet("QDateEdit { background-color: #2B2F3A; color: white; border-radius: 6px; padding: 4px; }");
    auto* toDate = new QDateEdit(QDate::currentDate(), reservedBox);
    toDate->setStyleSheet("QDateEdit { background-color: #2B2F3A; color: white; border-radius: 6px; padding: 4px; }");
    fromDate->setCalendarPopup(true);
    toDate->setCalendarPopup(true);
    fromDate->setDisplayFormat("yyyy-MM-dd");
    toDate->setDisplayFormat("yyyy-MM-dd");
    QLabel* fromLabel = new QLabel("From:");
    fromLabel->setStyleSheet("color: white;");
    QLabel* toLabel = new QLabel("To:");
    toLabel->setStyleSheet("color: white;");
    dateLayout->addWidget(fromLabel);
    dateLayout->addWidget(fromDate);
    dateLayout->addWidget(toLabel);
    dateLayout->addWidget(toDate);
    inputStack->addWidget(dateWidget);  // index 0
    // 검색어 입력창 (1개만 사용)
    auto* filterInput = new QLineEdit(reservedBox);
    filterInput->setStyleSheet(R"(
        QLineEdit {
            background-color: #2B2F3A;
            color: white;
            border: 1px solid #444;
            border-radius: 6px;
            padding: 4px 8px;
        }
    )");
    filterInput->setPlaceholderText("Enter keyword...");
    inputStack->addWidget(filterInput);  // index 1
    // button
    auto* searchButton = new QPushButton("🔍Search", reservedBox);
    searchButton->setStyleSheet(R"(
        QPushButton {
            background-color: #2B2F3A;
            color: white;
            padding: 6px 14px;
            border-radius: 6px;
        }
        QPushButton:hover {
            background-color: #3A3F4F;
        }
    )");
    auto* exportButton = new QPushButton("📁 Export", reservedBox);
    exportButton->setStyleSheet(R"(
        QPushButton {
            background-color: #00a63e;
            color: white;
            padding: 6px 14px;
            border-radius: 6px;
            font-weight: bold;
        }
        QPushButton:hover {
            background-color: #00A000;
        }
    )");
    m_refreshLogsButton = new QPushButton(" 🔄 ", reservedBox); // 🔄 새로고침 버튼 추가
    m_refreshLogsButton->setStyleSheet(R"(
        QPushButton {
            background-color: #2B2F3A;
            color: white;
            border-radius: 6px;
        }
        QPushButton:hover {
            background-color: #3A3F4F;
        }
    )");
    m_refreshLogsButton->setFixedWidth(30);
    m_refreshLogsButton->setToolTip("Reload Logs");
    filterLayout->addWidget(filterCombo);
    filterLayout->addWidget(inputStack, 1);
    filterLayout->addWidget(searchButton);
    filterLayout->addWidget(exportButton);
    filterLayout->addWidget(m_refreshLogsButton);
    reservedLayout->addLayout(filterLayout);
    // search and export
    m_filterCombo = filterCombo;
    m_inputStack = inputStack;
    m_filterInput = filterInput;
    m_fromDate = fromDate;
    m_toDate = toDate;
    connect(searchButton, &QPushButton::clicked, this, &MonitorWindow::onSearchLogs);
    connect(exportButton, &QPushButton::clicked, this, &MonitorWindow::onExportLogs);
    connect(m_refreshLogsButton, &QPushButton::clicked, this, [=]() {
        if (!m_currentUserId.isEmpty())
            loadUserLogsFromDatabase(m_currentUserId);  // ✅ 새로 로딩
    });
    // QLineEdit도 검색 트리거 가능하게
    connect(filterInput, &QLineEdit::returnPressed, this, &MonitorWindow::onSearchLogs);
    connect(filterCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [=](int index){
        if (index == 0) {
            inputStack->setCurrentIndex(0); // QDateEdit
        } else {
            inputStack->setCurrentIndex(1); // QLineEdit
        }
    });
    // Persistent 로그 테이블 추가
    persistentLogTable = new QTableWidget(0, 4, reservedBox);
    persistentLogTable->setStyleSheet(alertTable->styleSheet());
    persistentLogTable->setHorizontalHeaderLabels({tr("Time"), tr("Camera"), tr("Event"), tr("Status")});
    persistentLogTable->horizontalHeader()->setStretchLastSection(true);
    persistentLogTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
    reservedLayout->addWidget(persistentLogTable);
    vbox->addWidget(reservedBox, /*stretch=*/1);  // Smaller stretch factor

    dock->setWidget(cameraTabs);
    addDockWidget(Qt::RightDockWidgetArea, dock);

    // Set dock widget size to take about 1/3 of window width (but leave room for cameras)
    dock->setMinimumWidth(400);  // Slightly smaller minimum
    dock->setMaximumWidth(550);  // Set maximum to prevent it from taking too much space

    // Make the dock widget take up more space
    dock->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);

    // Force the dock to be wider but not too wide - will be applied after window is shown
    QTimer::singleShot(100, this, [this, dock]() {
        resizeDocks({dock}, {450}, Qt::Horizontal);  // Set dock to 450px width (smaller than before)
    });

    // Add initial log entry
    addLogEntry("System", "Initialization", "MonitorWindow started successfully");
    addLogEntry("Map", "Kakao Map", "Loading camera location map...");

    // 데이터 Tab 추가
    cameraTabs->addTab(container, tr("Data"));

    // 설정 Tab 추가
    cameraTabs->addTab(createSettingTab(), tr("Setting"));

    dock->setStyleSheet(R"(
        QDockWidget {
            font-size: 13px;
            background-color: #1E1F2A;
            color: white;
            border: 1px solid #333;
        }

        QDockWidget::title {
            text-align: left;
            padding-left: 10px;
            background: #1B1C27;
            font-weight: bold;
            height: 28px;
        }
    )");
    return dock;
}

void MonitorWindow::onSearchLogs() {
    if (!persistentLogTable || !m_filterCombo || !m_inputStack) return;

    QString field = m_filterCombo->currentText();
    QString keyword = m_filterInput->text().trimmed();
    QDate from = m_fromDate->date();
    QDate to = m_toDate->date();

    persistentLogTable->setRowCount(0);
    QSqlDatabase db = QSqlDatabase::database();
    if (!db.isOpen()) return;

    QString sql = "SELECT timestamp, source, event, status FROM logs WHERE user_id = ?";
    if (field == "Time") {
        sql += " AND date(timestamp) BETWEEN ? AND ?";
    } else if (!keyword.isEmpty()) {
        if (field == "Camera") sql += " AND source LIKE ?";
        else if (field == "Event") sql += " AND event LIKE ?";
        else if (field == "Status") sql += " AND status LIKE ?";
    }
    sql += " ORDER BY id DESC";

    QSqlQuery query(db);
    query.prepare(sql);
    query.addBindValue(m_currentUserId);

    if (field == "Time") {
        query.addBindValue(from.toString("yyyy-MM-dd"));
        query.addBindValue(to.toString("yyyy-MM-dd"));
    } else if (!keyword.isEmpty()) {
        query.addBindValue("%" + keyword + "%");
    }

    if (!query.exec()) {
        qWarning() << "Query failed:" << query.lastError().text();
        return;
    }

    while (query.next()) {
        int row = persistentLogTable->rowCount();
        persistentLogTable->insertRow(row);
        for (int i = 0; i < 4; ++i)
            persistentLogTable->setItem(row, i, new QTableWidgetItem(query.value(i).toString()));
    }

    persistentLogTable->scrollToTop();
}


void MonitorWindow::onExportLogs() {
    QString filePath = QFileDialog::getSaveFileName(this, "Export Logs", "", "CSV Files (*.csv)");
    if (filePath.isEmpty()) return;

    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) return;

    QTextStream out(&file);
    out << "Time,Camera,Event,Status\n";

    for (int i = 0; i < persistentLogTable->rowCount(); ++i) {
        for (int j = 0; j < 4; ++j) {
            auto* item = persistentLogTable->item(i, j);
            out << (item ? item->text() : "") << (j < 3 ? "," : "\n");
        }
    }

    file.close();
    QMessageBox::information(this, "Export Complete", "Logs exported successfully.");
}


void MonitorWindow::loadUserLogsFromDatabase(const QString& userId) {
    if (!persistentLogTable) return;

    persistentLogTable->setRowCount(0);  // 초기화

    QSqlDatabase db = QSqlDatabase::database();  // 기본 DB 연결 사용
    if (!db.isOpen()) {
        qWarning() << "DB not open while loading logs";
        return;
    }

    QSqlQuery query(db);
    query.prepare("SELECT timestamp, source, event, status FROM logs WHERE user_id = ? ORDER BY id ASC");
    query.addBindValue(userId);

    if (!query.exec()) {
        qWarning() << "Failed to query logs:" << query.lastError().text();
        return;
    }

    while (query.next()) {
        int row = persistentLogTable->rowCount();
        persistentLogTable->insertRow(row);

        persistentLogTable->setItem(row, 0, new QTableWidgetItem(query.value(0).toString())); // timestamp
        persistentLogTable->setItem(row, 1, new QTableWidgetItem(query.value(1).toString())); // source
        persistentLogTable->setItem(row, 2, new QTableWidgetItem(query.value(2).toString())); // event
        persistentLogTable->setItem(row, 3, new QTableWidgetItem(query.value(3).toString())); // status
    }

    persistentLogTable->scrollToBottom();
}

void MonitorWindow::startMonitoring() {
    statusBar()->showMessage(tr("Monitoring already active"));
    addLogEntry("System", "Start Monitoring", "Cameras are already streaming automatically");
}

void MonitorWindow::stopMonitoring() {
    statusBar()->showMessage(tr("Manual stop not available in automatic mode"));
    addLogEntry("System", "Stop Monitoring", "Cameras use automatic reconnection - manual stop disabled");
}

void MonitorWindow::takeSnapshot() {
    // Create snapshots directory if it doesn't exist
    QString snapshotDir = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation) + "/RTSP_Snapshots";
    QDir dir;
    if (!dir.exists(snapshotDir)) {
        dir.mkpath(snapshotDir);
    }

    QString timestamp = QDateTime::currentDateTime().toString("yyyy-MM-dd_hh-mm-ss");
    int savedCount = 0;

    // Take snapshot from camera 1 (specialized OpenCV client)
    if (camera1Client) {
        QPixmap currentFrame = camera1Client->getCurrentFrame();
        if (!currentFrame.isNull()) {
            QString filename = QString("%1/Camera1_%2.png")
            .arg(snapshotDir)
                .arg(timestamp);

            if (currentFrame.save(filename, "PNG")) {
                savedCount++;
            }
        }
    }

    // Take snapshots from cameras 2, 3, 4 (regular clients)
    for (int i = 0; i < rtspClients.size(); ++i) {
        RtspClient* client = rtspClients[i];
        if (client) {
            QPixmap currentFrame = client->getCurrentFrame();
            if (!currentFrame.isNull()) {
                QString filename = QString("%1/Camera%2_%3.png")
                .arg(snapshotDir)
                    .arg(i + 2)  // Camera numbers 2, 3, 4
                    .arg(timestamp);

                if (currentFrame.save(filename, "PNG")) {
                    savedCount++;
                }
            }
        }
    }

    QString message = QString("Saved %1 snapshots to %2").arg(savedCount).arg(snapshotDir);
    addLogEntry("System", "Take Snapshot", message);
    statusBar()->showMessage(tr("Snapshots saved: %1").arg(savedCount));

    if (savedCount > 0) {
        QMessageBox::information(this, tr("Snapshots Saved"),
                                 QString(tr("Saved %1 camera snapshots to:\n%2"))
                                     .arg(savedCount).arg(snapshotDir));
    } else {
        QMessageBox::warning(this, tr("No Snapshots"),
                             tr("No active video frames available for snapshot."));
    }
}

void MonitorWindow::refreshAll() {
    statusBar()->showMessage(tr("Automatic reconnection handles refresh"));
    addLogEntry("System", "Refresh All", "Cameras use automatic freeze detection and reconnection");
}

// Pi Camera Control Functions
void MonitorWindow::startPiCamera2() {
    if (rtspClients.size() > 0 && rtspClients[0]) {
        rtspClients[0]->startStream();
        addLogEntry("Camera 2", "Start Stream", "Pi Camera 2 stream started");
        statusBar()->showMessage(tr("Camera 2 started"));
    } else {
        addLogEntry("Camera 2", "Start Stream", "ERROR: Camera 2 not available");
        statusBar()->showMessage(tr("Camera 2 not available"));
    }
}

void MonitorWindow::stopPiCamera2() {
    if (rtspClients.size() > 0 && rtspClients[0]) {
        rtspClients[0]->stopStream();
        addLogEntry("Camera 2", "Stop Stream", "Pi Camera 2 stream stopped");
        statusBar()->showMessage(tr("Camera 2 stopped"));
    } else {
        addLogEntry("Camera 2", "Stop Stream", "ERROR: Camera 2 not available");
        statusBar()->showMessage(tr("Camera 2 not available"));
    }
}

void MonitorWindow::startPiCamera3() {
    if (rtspClients.size() > 1 && rtspClients[1]) {
        rtspClients[1]->startStream();
        addLogEntry("Camera 3", "Start Stream", "Pi Camera 3 stream started");
        statusBar()->showMessage(tr("Camera 3 started"));
    } else {
        addLogEntry("Camera 3", "Start Stream", "ERROR: Camera 3 not available");
        statusBar()->showMessage(tr("Camera 3 not available"));
    }
}

void MonitorWindow::stopPiCamera3() {
    if (rtspClients.size() > 1 && rtspClients[1]) {
        rtspClients[1]->stopStream();
        addLogEntry("Camera 3", "Stop Stream", "Pi Camera 3 stream stopped");
        statusBar()->showMessage(tr("Camera 3 stopped"));
    } else {
        addLogEntry("Camera 3", "Stop Stream", "ERROR: Camera 3 not available");
        statusBar()->showMessage(tr("Camera 3 not available"));
    }
}

void MonitorWindow::startPiCamera4() {
    if (rtspClients.size() > 2 && rtspClients[2]) {
        rtspClients[2]->startStream();
        addLogEntry("Camera 4", "Start Stream", "Pi Camera 4 stream started");
        statusBar()->showMessage(tr("Camera 4 started"));
    } else {
        addLogEntry("Camera 4", "Start Stream", "ERROR: Camera 4 not available");
        statusBar()->showMessage(tr("Camera 4 not available"));
    }
}

void MonitorWindow::stopPiCamera4() {
    if (rtspClients.size() > 2 && rtspClients[2]) {
        rtspClients[2]->stopStream();
        addLogEntry("Camera 4", "Stop Stream", "Pi Camera 4 stream stopped");
        statusBar()->showMessage(tr("Camera 4 stopped"));
    } else {
        addLogEntry("Camera 4", "Stop Stream", "ERROR: Camera 4 not available");
        statusBar()->showMessage(tr("Camera 4 not available"));
    }
}

void MonitorWindow::startAllPiCameras() {
    int startedCount = 0;

    // Start cameras 2, 3, and 4 (Pi cameras)
    for (int i = 0; i < rtspClients.size() && i < 3; ++i) {
        if (rtspClients[i]) {
            rtspClients[i]->startStream();
            startedCount++;
        }
    }

    QString message = QString("Started %1 Pi cameras").arg(startedCount);
    addLogEntry("Pi Cameras", "Start All", message);
    statusBar()->showMessage(tr("Pi cameras started: %1").arg(startedCount));
}

void MonitorWindow::stopAllPiCameras() {
    int stoppedCount = 0;

    // Stop cameras 2, 3, and 4 (Pi cameras)
    for (int i = 0; i < rtspClients.size() && i < 3; ++i) {
        if (rtspClients[i]) {
            rtspClients[i]->stopStream();
            stoppedCount++;
        }
    }

    QString message = QString("Stopped %1 Pi cameras").arg(stoppedCount);
    addLogEntry("Pi Cameras", "Stop All", message);
    statusBar()->showMessage(tr("Pi cameras stopped: %1").arg(stoppedCount));
}

// UI 실시간 알림 추가
void MonitorWindow::addLogEntry(const QString& source, const QString& event, const QString& status) {
    if (!alertTable) return;

    int row = alertTable->rowCount();
    alertTable->insertRow(row);

    QString currentTime = QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss");
    alertTable->setItem(row, 0, new QTableWidgetItem(currentTime));
    alertTable->setItem(row, 1, new QTableWidgetItem(source));
    alertTable->setItem(row, 2, new QTableWidgetItem(event));
    alertTable->setItem(row, 3, new QTableWidgetItem(status));

    // Auto-scroll to the latest entry
    alertTable->scrollToBottom();

    // 사용자 ID가 없다면 기록하지 않음
    if (m_currentUserId.isEmpty()) return;

    // 기존 users.db에 접근
    QSqlDatabase db = QSqlDatabase::database();  // 기본 connection 사용
    if (!db.isOpen()) {
        qWarning() << "users.db not open for logging!";
        return;
    }

    QSqlQuery query(db);
    query.prepare("INSERT INTO logs (user_id, timestamp, source, event, status) VALUES (?, ?, ?, ?, ?)");
    query.addBindValue(m_currentUserId);
    query.addBindValue(currentTime);
    query.addBindValue(source);
    query.addBindValue(event);
    query.addBindValue(status);

    if (!query.exec()) {
        qWarning() << "Failed to insert log:" << query.lastError().text();
    }
}

// MapBridge 구현
void MapBridge::updateCameraStatus(int cameraId, const QString& status) {
    emit cameraStatusChanged(cameraId, status);
    qDebug() << "[monitorwindow] JavaScript received cameraId";

}

void MapBridge::setCameraLocations() {
    // JavaScript로 카메라 위치 설정 신호 보내기
}

void MapBridge::resetMapView() {
    // JavaScript의 resetToDefaultView 함수를 호출하는 신호 발생
    emit mapResetRequested();
}

// Kakao Map 설정
void MonitorWindow::setupKakaoMap() {
    // 웹 채널 설정
    webChannel = new QWebChannel(this);
    mapBridge = new MapBridge();
    webChannel->registerObject("mapBridge", mapBridge);

    // HTML 콘텐츠 생성 (Fallback map with Kakao API)
    QString htmlContent = QString(
                              R"(<!DOCTYPE html>
    <html>
    <head>
        <meta charset="UTF-8">
        <title>Camera Location Map</title>
        <meta name="viewport" content="width=device-width, initial-scale=1.0">
        <script type="text/javascript" src="https://dapi.kakao.com/v2/maps/sdk.js?appkey=e159aa60d1ae6a6cd1f785e5f8cb3934&autoload=false"></script>
        <script src="qrc:///qtwebchannel/qwebchannel.js"></script>
        <style>
            body { margin: 0; padding: 0; background: #f0f0f0; font-family: Arial, sans-serif; }
            #map { width: 100%; height: 100vh; background: #e8f4f8; position: relative; }
            #fallback-message {
                position: absolute; top: 20px; left: 20px; right: 20px;
                background: white; padding: 15px; border-radius: 8px;
                box-shadow: 0 2px 10px rgba(0,0,0,0.1); z-index: 1000;
            }
            .camera-marker {
                position: absolute; width: 40px; height: 40px;
                border-radius: 50%; border: 3px solid white;
                display: flex; align-items: center; justify-content: center;
                font-size: 18px; cursor: pointer; box-shadow: 0 2px 8px rgba(0,0,0,0.3);
                transition: transform 0.2s;
            }
            .camera-marker:hover { transform: scale(1.1); }
            .status-normal { background: #00cc00; }
            .status-crowded { background: #ee0000; }
            .status-warning { background: #ffaa00; }
            .camera-info {
                position: absolute; background: white; border-radius: 5px;
                padding: 10px 15px; box-shadow: 0 3px 12px rgba(0,0,0,0.2);
                font-size: 13px; border: 1px solid #ddd; min-width: 150px;
                display: none; z-index: 1001;
            }
            .camera-title { font-weight: bold; margin-bottom: 5px; color: #333; }
            .status-text { font-weight: bold; }
            .status-normal .status-text { color: #00aa00; }
            .status-crowded .status-text { color: #dd0000; }
            .status-warning .status-text { color: #ff8800; }
        </style>
    </head>
    <body>
        <div id="map"></div>
        <script>
            console.log('Starting camera map initialization...');)"
                              ) + QString(
                              R"(
            // 기본 지도 중심점 - 더현대몰 중앙 (더 정확한 건물 중심)
            const DEFAULT_CENTER = {
                lat: 37.285292,
                lng: 127.057492,
                level: 2
            };

            // 카메라 위치 데이터 (스크린샷의 파란 원 위치에 맞춰 조정)
            const cameraLocations = [
                { id: 1, name: 'Camera 1 (Center)', lat: 37.285125, lng: 127.057465, x: 50, y: 50, status: 'normal' },
                { id: 2, name: 'Camera 2 (West)', lat: 37.285135, lng: 127.056794, x: 50, y: 50, status: 'normal' },
                { id: 3, name: 'Camera 3 (North)', lat: 37.285642, lng: 127.057418, x: 50, y: 50, status: 'normal' },
                { id: 4, name: 'Camera 4 (East)', lat: 37.285199, lng: 127.058082, x: 50, y: 50, status: 'normal' },
            ];

            let map = null;
            let markers = {};
            let activeInfo = null;
            let activeInfoWindow = null;
            let usingFallback = false;)"
                              ) + QString(
                              R"(
            // Kakao 지도 초기화
            function initKakaoMap() {
                console.log('Initializing Kakao map...');

                const container = document.getElementById('map');
                const options = {
                    center: new window.kakao.maps.LatLng(DEFAULT_CENTER.lat, DEFAULT_CENTER.lng),
                    level: DEFAULT_CENTER.level
                };

                map = new window.kakao.maps.Map(container, options);
                map.addOverlayMapTypeId(window.kakao.maps.MapTypeId.TRAFFIC);

                const mapTypeControl = new window.kakao.maps.MapTypeControl();
                map.addControl(mapTypeControl, window.kakao.maps.ControlPosition.TOPRIGHT);

                const zoomControl = new window.kakao.maps.ZoomControl();
                map.addControl(zoomControl, window.kakao.maps.ControlPosition.RIGHT);

                addResetButton();

                cameraLocations.forEach(camera => {
                    addKakaoMarker(camera);
                });

                console.log('Kakao map initialized with', cameraLocations.length, 'cameras');
            })"
                              ) + QString(
                              R"(
            // Kakao 마커 추가
            function addKakaoMarker(camera) {
                const position = new window.kakao.maps.LatLng(camera.lat, camera.lng);
                const markerImageSrc = getMarkerImageUrl(camera.status);
                const markerImage = new window.kakao.maps.MarkerImage(markerImageSrc, new window.kakao.maps.Size(40, 40));

                const marker = new window.kakao.maps.Marker({
                    position: position,
                    image: markerImage,
                    title: camera.name
                });

                marker.setMap(map);

                const infoWindow = new window.kakao.maps.InfoWindow({
                    content: getKakaoInfoContent(camera)
                });

                window.kakao.maps.event.addListener(marker, 'mouseover', function() {
                    console.log('Camera marker mouseover:', camera.id);
                    if (activeInfoWindow) {
                        activeInfoWindow.close();
                    }
                    infoWindow.open(map, marker);
                    activeInfoWindow = infoWindow;
                });

                window.kakao.maps.event.addListener(marker, 'mouseout', function() {
                    console.log('Camera marker mouseout:', camera.id);
                    if (activeInfoWindow === infoWindow) {
                        infoWindow.close();
                        activeInfoWindow = null;
                    }
                });

                markers[camera.id] = { marker, infoWindow, camera };
            })"
                              ) + QString(
                              R"(
            // Kakao 인포윈도우 내용
            function getKakaoInfoContent(camera) {
                return `
                    <div style="padding: 8px 10px; width: 160px; box-sizing: border-box; font-size: 12px; line-height: 1.3;">
                        <div style="font-weight: bold; margin-bottom: 4px; font-size: 13px; white-space: nowrap; overflow: hidden; text-overflow: ellipsis;">${camera.name}</div>
                        <div style="color: #666; font-size: 10px; margin-bottom: 3px; white-space: nowrap;">📍 ${camera.lat.toFixed(6)}, ${camera.lng.toFixed(6)}</div>
                        <div style="color: ${getStatusColor(camera.status)}; font-weight: bold; font-size: 11px; white-space: nowrap;">🚥 ${getStatusText(camera.status)}</div>
                    </div>
                `;
            }

            // 마커 이미지 URL 생성 (SVG)
            function getMarkerImageUrl(status) {
                const color = getStatusColor(status);
                const svg = `
                    <svg width="40" height="40" xmlns="http://www.w3.org/2000/svg">
                        <circle cx="20" cy="20" r="18" fill="${color}" stroke="white" stroke-width="3"/>
                        <text x="20" y="26" text-anchor="middle" font-size="16" fill="white">📹</text>
                    </svg>
                `;
                return 'data:image/svg+xml;charset=utf-8,' + encodeURIComponent(svg);
            }

            // 상태별 색상
            function getStatusColor(status) {
                const colors = {
                    normal: '#00cc00',
                    crowded: '#ee0000',
                    warning: '#ffaa00',
                    unknown: '#888888'
                };
                return colors[status] || colors.unknown;
            })"
                              ) + QString(
                              R"(
            // 기본 위치로 돌아가기 버튼 추가
            function addResetButton() {
                const resetControlDiv = document.createElement('div');
                resetControlDiv.style.position = 'absolute';
                resetControlDiv.style.top = '10px';
                resetControlDiv.style.left = '10px';
                resetControlDiv.style.zIndex = '1000';

                const resetButton = document.createElement('button');
                resetButton.innerHTML = '🏢 갤러리아몰';
                resetButton.title = '기본 위치로 돌아가기 (갤러리아몰 중심)';
                resetButton.style.cssText = `
                    background: white; border: 2px solid #333; border-radius: 5px; padding: 8px 12px;
                    font-size: 12px; font-weight: bold; cursor: pointer;
                    box-shadow: 0 2px 5px rgba(0,0,0,0.2); transition: all 0.2s;
                `;

                resetButton.addEventListener('mouseenter', function() {
                    this.style.backgroundColor = '#f0f0f0';
                    this.style.transform = 'scale(1.05)';
                });

                resetButton.addEventListener('mouseleave', function() {
                    this.style.backgroundColor = 'white';
                    this.style.transform = 'scale(1)';
                });

                resetButton.addEventListener('click', function() {
                    resetToDefaultView();
                });

                resetControlDiv.appendChild(resetButton);
                document.getElementById('map').appendChild(resetControlDiv);
            })"
                              ) + QString(
                              R"(
            // 기본 위치로 돌아가기 함수
            function resetToDefaultView() {
                if (map) {
                    console.log('Resetting to default view: 갤러리아몰');
                    const defaultCenter = new window.kakao.maps.LatLng(DEFAULT_CENTER.lat, DEFAULT_CENTER.lng);
                    map.setCenter(defaultCenter);
                    map.setLevel(DEFAULT_CENTER.level);
                    map.panTo(defaultCenter);
                    console.log(`Map reset to: ${DEFAULT_CENTER.lat}, ${DEFAULT_CENTER.lng}, level: ${DEFAULT_CENTER.level}`);
                }
            }

            // 간단한 맵 초기화 (외부 API 없음)
            function initFallbackMap() {
                console.log('Initializing fallback map...');
                usingFallback = true;
                const mapContainer = document.getElementById('map');

                const messageDiv = document.createElement('div');
                messageDiv.id = 'fallback-message';
                messageDiv.innerHTML = `
                    <h3 style="margin: 0 0 10px 0; color: #333;">📹 Camera Location Map (Offline Mode)</h3>
                    <p style="margin: 0; color: #666; font-size: 12px;">Camera positions and real-time status monitoring</p>
                `;
                mapContainer.appendChild(messageDiv);

                cameraLocations.forEach(camera => {
                    addFallbackMarker(camera);
                });

                console.log('Fallback map initialized with', cameraLocations.length, 'cameras');
            })"
                              ) + QString(
                              R"(
            // 간단한 마커 추가 (위치는 퍼센트)
            function addFallbackMarker(camera) {
                const mapContainer = document.getElementById('map');

                const marker = document.createElement('div');
                marker.className = `camera-marker status-${camera.status}`;
                marker.style.left = camera.x + '%';
                marker.style.top = camera.y + '%';
                marker.innerHTML = '📹';
                marker.title = camera.name;

                const infoDiv = document.createElement('div');
                infoDiv.className = `camera-info status-${camera.status}`;
                infoDiv.style.left = (camera.x + 5) + '%';
                infoDiv.style.top = (camera.y - 5) + '%';
                infoDiv.innerHTML = `
                    <div class="camera-title">${camera.name}</div>
                    <div class="status-text">Status: ${getStatusText(camera.status)}</div>
                `;

                marker.addEventListener('click', function(e) {
                    e.stopPropagation();
                    document.querySelectorAll('.camera-info').forEach(info => {
                        info.style.display = 'none';
                    });
                    infoDiv.style.display = 'block';
                    activeInfo = infoDiv;
                });

                mapContainer.appendChild(marker);
                mapContainer.appendChild(infoDiv);
                markers[camera.id] = { marker, infoDiv, camera };
            })"
                              ) + QString(
                              R"(
            // 상태 텍스트 반환
            function getStatusText(status) {
                const statusTexts = {
                    normal: 'Normal',
                    crowded: 'Crowded',
                    warning: 'Warning',
                    unknown: 'Unknown'
                };
                return statusTexts[status] || 'Unknown';
            }

            // 카메라 상태 업데이트 함수 (Kakao 지도용)
            function updateKakaoMarker(cameraId, newStatus) {
                const markerData = markers[cameraId];
                if (markerData && !usingFallback) {
                    const { marker, infoWindow, camera } = markerData;
                    camera.status = newStatus;

                    const newMarkerImage = new window.kakao.maps.MarkerImage(
                        getMarkerImageUrl(newStatus),
                        new window.kakao.maps.Size(40, 40)
                    );
                    marker.setImage(newMarkerImage);
                    infoWindow.setContent(getKakaoInfoContent(camera));

                    if (activeInfoWindow === infoWindow) {
                        activeInfoWindow = infoWindow;
                    }
                    console.log('Kakao marker', cameraId, 'updated to', newStatus);
                }
            })"
                              ) + QString(
                              R"(
            // 카메라 상태 업데이트 함수 (Fallback 지도용)
            function updateFallbackMarker(cameraId, newStatus) {
                const markerData = markers[cameraId];
                if (markerData && usingFallback) {
                    const { marker, infoDiv, camera } = markerData;
                    camera.status = newStatus;

                    marker.className = `camera-marker status-${newStatus}`;
                    infoDiv.className = `camera-info status-${newStatus}`;
                    infoDiv.innerHTML = `
                        <div class="camera-title">${camera.name}</div>
                        <div class="status-text">Status: ${getStatusText(newStatus)}</div>
                    `;
                    console.log('Fallback marker', cameraId, 'updated to', newStatus);
                }
            }

            // 통합 카메라 상태 업데이트 함수
            function updateCameraStatus(cameraId, newStatus) {
                console.log('Updating camera', cameraId, 'to status:', newStatus);
                if (usingFallback) {
                    updateFallbackMarker(cameraId, newStatus);
                } else {
                    updateKakaoMarker(cameraId, newStatus);
                }
            }

            // 맵 영역 클릭 시 정보창 숨기기
            document.addEventListener('click', function() {
                if (activeInfo) {
                    activeInfo.style.display = 'none';
                    activeInfo = null;
                }
            });)"
                              ) + QString(
                              R"(
            // Qt와 통신 설정
            new QWebChannel(qt.webChannelTransport, function (channel) {
                console.log('QWebChannel connected');
                window.mapBridge = channel.objects.mapBridge;

                if (mapBridge) {
                    mapBridge.cameraStatusChanged.connect(function(cameraId, status) {
                        updateCameraStatus(cameraId, status);
                    });

                    mapBridge.mapResetRequested.connect(function() {
                        resetToDefaultView();
                    });

                    console.log('Map bridge connected successfully');
                }
            });

            // 지도 초기화 시도
            function tryInitializeMap() {
                console.log('Attempting to initialize map...');

                if (typeof window.kakao !== 'undefined' && window.kakao.maps) {
                    console.log('Kakao Maps API detected, loading map...');
                    window.kakao.maps.load(function() {
                        try {
                            initKakaoMap();
                            console.log('Kakao Maps loaded successfully');
                        } catch (error) {
                            console.log('Kakao Maps failed to initialize:', error);
                            initFallbackMap();
                        }
                    });
                } else {
                    console.log('Kakao Maps API not available, using fallback');
                    initFallbackMap();
                }
            }

            // 페이지 로드 후 초기화
            document.addEventListener('DOMContentLoaded', function() {
                console.log('DOM loaded, initializing map...');
                setTimeout(tryInitializeMap, 100);
            });

            // 추가 로드 이벤트
            window.addEventListener('load', function() {
                console.log('Window loaded');
                setTimeout(function() {
                    if (!map && !document.getElementById('fallback-message')) {
                        console.log('No map initialized yet, trying again...');
                        tryInitializeMap();
                    }
                }, 500);
            });
        </script>
    </body>
    </html>)"
                              );

    // 웹뷰에 HTML 로드
    mapView->page()->setWebChannel(webChannel);
    mapView->setHtml(htmlContent);
    qDebug() << "Kakao Map setup completed";
    mapView->update();
    mapView->repaint();
    mapView->show();
}

int MonitorWindow::getTodayOvercrowdedCount() {
    QSqlQuery query(QSqlDatabase::database());
    query.prepare(R"(
        SELECT COUNT(*) FROM logs
        WHERE event = 'Overcrowded'
        AND DATE(timestamp) = DATE('now', 'localtime')
        AND user_id = ?
    )");
    query.addBindValue(m_currentUserId);

    if (query.exec() && query.next())
        return query.value(0).toInt();

    qWarning() << "[OvercrowdCount] DB 쿼리 실패:" << query.lastError().text();
    return 0;
}

// 혼잡도 threshold
QString MonitorWindow::determineStatusFromCrowdCount(int count) {
    if (count >= red)
        return "crowded";   // 빨간색
    else if (count >= yellow)
        return "warning";   // 주황색
    else
        return "normal";    // 초록색
}

void MonitorWindow::updateGateTotalCount() {
    int g1 = gate1CountLabel ? gate1CountLabel->text().remove("명").toInt() : 0;
    int g2 = gate2CountLabel ? gate2CountLabel->text().remove("명").toInt() : 0;
    int g3 = gate3CountLabel ? gate3CountLabel->text().remove("명").toInt() : 0;
    int total = g1 + g2 + g3;

    if (gateCountLabel)
        gateCountLabel->setText(QString::number(total));
}

// Kakao map and log update
void MonitorWindow::updateCameraCrowdCount(int cameraId, int count) {
    QString status = determineStatusFromCrowdCount(count);

    if (mapBridge)
        mapBridge->updateCameraStatus(cameraId, status);

    QString cameraName;
    // UI 텍스트 업데이트
    switch (cameraId) {
    case 1:
        cameraName = "Indoor";
        if (indoorCountLabel) {
            indoorCountLabel->setText(QString::number(count));
        }
        // progressbar
        if (m_crowdProgressBar)
            m_crowdProgressBar->setValue(count);
        if (percentLabel)
            percentLabel->setText(QString("%1%").arg(count));

        // 2. 과밀도 이벤트 → 80% 넘었으면 DB에 기록
        if (count > 80) {
            addLogEntry("Indoor", "Overcrowded", QString("Indoor density %1% exceeded threshold").arg(count));
        }
        break;
    case 2:
        cameraName = "Gate 1";
        if (gate1CountLabel)
            gate1CountLabel->setText(QString("%1명").arg(count));
        updateGateTotalCount();
        break;
    case 3:
        cameraName = "Gate 2";
        if (gate2CountLabel)
            gate2CountLabel->setText(QString("%1명").arg(count));
        updateGateTotalCount();
        break;
    case 4:
        cameraName = "Gate 3";
        if (gate3CountLabel)
            gate3CountLabel->setText(QString("%1명").arg(count));
        updateGateTotalCount();
        break;
    }

    addLogEntry(cameraName, "Number of people", QString("Updated count/status: %1 (%2)").arg(count).arg(status));
}
