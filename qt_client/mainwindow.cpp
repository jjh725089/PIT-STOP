// // #include "mainwindow.h"
// // #include "signupwindow.h"
// // #include "monitorwindow.h"
// // #include <QMessageBox>
// // #include <QFormLayout>
// // #include <QHBoxLayout>
// // #include <QApplication>
// // #include <QJsonObject>
// // #include <QJsonArray>
// // #include <QJsonDocument>

// // MainWindow::MainWindow(QWidget *parent)
// //     : QMainWindow(parent)
// //     , m_dbManager("users.db")
// //     , idLineEdit(new QLineEdit(this))
// //     , passwordLineEdit(new QLineEdit(this))
// //     , loginButton(new QPushButton(tr("로그인"), this))
// //     , signupButton(new QPushButton(tr("회원가입"), this))
// //     , m_mqttClient(new Mqtt(this))
// // {
// //     setWindowTitle(tr("로그인"));
// //     setFixedSize(300, 150);

// //     // 비밀번호는 마스킹
// //     passwordLineEdit->setEchoMode(QLineEdit::Password);

// //     // placeholder
// //     idLineEdit->setPlaceholderText(tr("아이디 입력"));
// //     passwordLineEdit->setPlaceholderText(tr("비밀번호 입력"));

// //     // Form 레이아웃에 ID/PW 필드 추가
// //     auto *formLayout = new QFormLayout;
// //     formLayout->addRow(tr("아이디:"),      idLineEdit);
// //     formLayout->addRow(tr("비밀번호:"),  passwordLineEdit);

// //     // 버튼 레이아웃
// //     auto *buttonLayout = new QHBoxLayout;
// //     buttonLayout->addStretch();
// //     buttonLayout->addWidget(loginButton);
// //     buttonLayout->addWidget(signupButton);

// //     // 전체 레이아웃
// //     auto *mainLayout = new QVBoxLayout;
// //     mainLayout->addLayout(formLayout);
// //     mainLayout->addLayout(buttonLayout);

// //     // 중앙 위젯 설정
// //     QWidget *central = new QWidget(this);
// //     central->setLayout(mainLayout);
// //     setCentralWidget(central);

// //     // 시그널 연결
// //     connect(loginButton,  &QPushButton::clicked, this, &MainWindow::onLoginClicked);
// //     connect(signupButton, &QPushButton::clicked, this, &MainWindow::onSignupClicked);
// // }

// // void MainWindow::onLoginClicked()
// // {
// //     const QString id = idLineEdit->text().trimmed();
// //     const QString pw = passwordLineEdit->text();

// //     if (id.isEmpty() || pw.isEmpty()) {
// //         QMessageBox::warning(this, tr("입력 오류"), tr("아이디와 비밀번호를 모두 입력하세요."));
// //         return;
// //     }

// //     if (m_dbManager.loginUser(id, pw)) {
// //         QMessageBox::information(this, tr("로그인"), tr("로그인에 성공했습니다."));

// //         // 로그인 성공 후 mqtt로 바로 메시지 발행 시도
// //         if (m_mqttClient->state() == QMqttClient::Connected) {
// //             publishCameraCoordinates(id);
// //         } else {
// //             // 연결 준비 중이라면: connected 시그널에서 발행하도록 연결
// //             connect(m_mqttClient, &Mqtt::connected, this, [this, id]() {
// //                 publishCameraCoordinates(id);
// //             }, Qt::UniqueConnection);
// //         }

// //         // Open MonitorWindow with integrated RTSP streaming
// //         auto *monitor = new MonitorWindow();
// //         monitor->setCurrentUserId(id); // Pass ID for logout/delete

// //         // 로그인 후 로그 불러오기
// //         QMetaObject::invokeMethod(monitor, [monitor, id]() {
// //             monitor->loadUserLogsFromDatabase(id);
// //         }, Qt::QueuedConnection);

// //         monitor->showMaximized();
// //         this->close();
// //     } else {
// //         QMessageBox::warning(this, tr("로그인 실패"), tr("아이디 혹은 비밀번호를 잘못 입력했습니다."));
// //         // Close application on login failure
// //         // QApplication::quit();
// //     }
// // }

// // void MainWindow::onSignupClicked()
// // {
// //     SignupWindow dlg(this);
// //     if (dlg.exec() == QDialog::Accepted) {
// //         const QString newId = dlg.userId();
// //         const QString newPw = dlg.userPassword();
// //         // 카메라 좌표값도 받아오기
// //         double cam1x = dlg.userCamera1X();
// //         double cam1y = dlg.userCamera1Y();
// //         double cam2x = dlg.userCamera2X();
// //         double cam2y = dlg.userCamera2Y();
// //         double cam3x = dlg.userCamera3X();
// //         double cam3y = dlg.userCamera3Y();
// //         if (m_dbManager.registerUser(newId, newPw, cam1x, cam1y, cam2x, cam2y, cam3x, cam3y)) {
// //             QMessageBox::information(this, tr("회원가입"), tr("회원가입에 성공했습니다."));
// //         } else {
// //             QMessageBox::warning(this, tr("회원가입 실패"), tr("이미 존재하는 아이디입니다."));
// //         }
// //         // if (m_dbManager.registerUser(newId, newPw)) {
// //         //     QMessageBox::information(this, tr("회원가입"), tr("회원가입에 성공했습니다."));
// //         // } else {
// //         //     QMessageBox::warning(this, tr("회원가입 실패"), tr("이미 존재하는 아이디입니다."));
// //         // }
// //     }
// // }

// // void MainWindow::publishCameraCoordinates(const QString &userId)
// // {
// //     DatabaseManager::CameraCoordinates coords;
// //     if (m_dbManager.getCameraCoordinates(userId, coords)) {
// //         QJsonArray camArray;
// //         camArray.append(QJsonObject{{"x", coords.cam1x}, {"y", coords.cam1y}});
// //         camArray.append(QJsonObject{{"x", coords.cam2x}, {"y", coords.cam2y}});
// //         camArray.append(QJsonObject{{"x", coords.cam3x}, {"y", coords.cam3y}});

// //         QJsonObject jsonObj;
// //         jsonObj["cameras"] = camArray;

// //         QJsonDocument jsonDoc(jsonObj);
// //         QByteArray payload = jsonDoc.toJson(QJsonDocument::Compact);

// //         QString topic = QString("qt/data/exits");
// //         qDebug() << "MQTT 메시지 발행";
// //         if (m_mqttClient) {
// //             if (!m_mqttClient->publishMessage(topic, payload)) {
// //                 qWarning() << "MQTT 메시지 발행 실패";
// //             }
// //         }
// //     }
// //     else {
// //         qWarning() << "사용자 카메라 좌표 조회 실패";
// //     }
// // }
// #include "mainwindow.h"
// #include "mqtt.h"
// #include "signupwindow.h"
// #include "monitorwindow.h"
// #include <QMessageBox>
// #include <QFormLayout>
// #include <QHBoxLayout>
// #include <QApplication>
// #include <QJsonObject>
// #include <QJsonArray>
// #include <QJsonDocument>
// #include <QLabel>
// #include <QPixmap>

// MainWindow::MainWindow(QWidget *parent)
//     : QMainWindow(parent)
//     , m_dbManager("users.db")
//     , idLineEdit(new QLineEdit(this))
//     , passwordLineEdit(new QLineEdit(this))
//     , loginButton(new QPushButton(tr("로그인"), this))
//     , signupButton(new QPushButton(tr("회원가입"), this))
//     , m_mqttClient(new Mqtt(this))
//     , loginFrame(new QFrame(this))  //로그인 창 수정 - jjh
// {
//     setWindowTitle(tr("로그인"));
//     setFixedSize(880, 540);  // 창 너비 조정

//     // --- 입력 필드 생성 및 설정 ---
//     idLineEdit = new QLineEdit;
//     passwordLineEdit = new QLineEdit;
//     loginButton = new QPushButton("로그인");
//     signupButton = new QPushButton("회원가입");
//     loginFrame = new QFrame;

//     passwordLineEdit->setEchoMode(QLineEdit::Password);
//     idLineEdit->setPlaceholderText(tr("아이디"));
//     passwordLineEdit->setPlaceholderText(tr("비밀번호"));

//     QString lineEditStyle =
//         "QLineEdit {"
//         "    background-color: #E8E8E8;"
//         "    border: none;"
//         "    border-radius: 4px;"
//         "    padding: 8px 10px;"
//         "    color: #333333;"
//         "    font-size: 12px;"
//         "    font-weight: normal;"
//         "}"
//         "QLineEdit:focus {"
//         "    background-color: #DDDDDD;"
//         "}";

//     idLineEdit->setStyleSheet(lineEditStyle);
//     passwordLineEdit->setStyleSheet(lineEditStyle);

//     QString buttonStyle =
//         "QPushButton {"
//         "    background-color: #E8E8E8;"
//         "    border: none;"
//         "    border-radius: 4px;"
//         "    padding: 8px 15px;"
//         "    color: #333333;"
//         "    font-weight: bold;"
//         "    font-size: 12px;"
//         "}"
//         "QPushButton:hover {"
//         "    background-color: #DDDDDD;"
//         "}"
//         "QPushButton:pressed {"
//         "    background-color: #CCCCCC;"
//         "}";

//     loginButton->setStyleSheet(buttonStyle);
//     signupButton->setStyleSheet(buttonStyle);

//     // --- 로그인 프레임 내부 레이아웃 ---
//     auto *inputLayout = new QVBoxLayout;
//     inputLayout->setSpacing(10);
//     inputLayout->addWidget(idLineEdit);
//     inputLayout->addWidget(passwordLineEdit);

//     auto *buttonLayout = new QHBoxLayout;
//     buttonLayout->setSpacing(10);
//     buttonLayout->addWidget(loginButton);
//     buttonLayout->addWidget(signupButton);

//     auto *loginLayout = new QVBoxLayout(loginFrame);
//     loginLayout->setSpacing(15);
//     loginLayout->setContentsMargins(0, 0, 0, 0);
//     loginLayout->addLayout(inputLayout);
//     loginLayout->addLayout(buttonLayout);

//     loginFrame->setFixedWidth(240);
//     loginFrame->setStyleSheet("QFrame { background-color: transparent; }");

//     // --- 왼쪽 패널 (주황 배경) ---
//     QWidget *leftPanel = new QWidget;
//     leftPanel->setFixedWidth(300);
//     leftPanel->setStyleSheet("background-color: #f37321;");
//     QVBoxLayout *leftLayout = new QVBoxLayout(leftPanel);
//     leftLayout->setAlignment(Qt::AlignCenter);
//     leftLayout->addWidget(loginFrame);

//     // --- 오른쪽 이미지 삽입 ---
//     QLabel *imageLabel = new QLabel;
//     imageLabel->setAlignment(Qt::AlignCenter);
//     imageLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
//     imageLabel->setStyleSheet("background-color: white;");  // 이미지 위젯 배경 흰색

//     QPixmap backgroundImage(":/images/images/login_image.png");  // qrc에 등록된 경로
//     if (!backgroundImage.isNull()) {
//         QPixmap scaledImage = backgroundImage.scaled(
//             560, 480, Qt::KeepAspectRatio, Qt::SmoothTransformation
//             );
//         imageLabel->setPixmap(scaledImage);
//     } else {
//         imageLabel->setText("이미지를 불러올 수 없습니다");
//         imageLabel->setStyleSheet("color: #999999; font-size: 14px;");
//     }

//     // --- 메인 레이아웃 ---
//     QHBoxLayout *mainLayout = new QHBoxLayout;
//     mainLayout->setContentsMargins(0, 0, 0, 0);
//     mainLayout->setSpacing(0);
//     mainLayout->addWidget(leftPanel);
//     mainLayout->addWidget(imageLabel);

//     QWidget *central = new QWidget(this);
//     central->setLayout(mainLayout);
//     central->setStyleSheet("background-color: white;");  // ✨ 배경 흰색 설정
//     setCentralWidget(central);

//     // --- 시그널 연결 ---
//     connect(loginButton, &QPushButton::clicked, this, &MainWindow::onLoginClicked);
//     connect(signupButton, &QPushButton::clicked, this, &MainWindow::onSignupClicked);
// }


// void MainWindow::onLoginClicked()
// {
//     const QString id = idLineEdit->text().trimmed();
//     const QString pw = passwordLineEdit->text();

//     if (id.isEmpty() || pw.isEmpty()) {
//         QMessageBox::warning(this, tr("입력 오류"), tr("아이디와 비밀번호를 모두 입력하세요."));
//         return;
//     }

//     if (m_dbManager.loginUser(id, pw)) {
//         QMessageBox::information(this, tr("로그인"), tr("로그인에 성공했습니다."));

//         // 로그인 성공 후 mqtt로 바로 메시지 발행 시도
//         if (m_mqttClient->state() == QMqttClient::Connected) {
//             publishCameraCoordinates(id);
//         } else {
//             // 연결 준비 중이라면: connected 시그널에서 발행하도록 연결
//             connect(m_mqttClient, &Mqtt::connected, this, [this, id]() {
//                 publishCameraCoordinates(id);
//             }, Qt::UniqueConnection);
//         }

//         // Open MonitorWindow with integrated RTSP streaming
//         auto *monitor = new MonitorWindow(id);
//         monitor->setCurrentUserId(id); // Pass ID for logout/delete

//         // 로그인 후 로그 불러오기
//         QMetaObject::invokeMethod(monitor, [monitor, id]() {
//             monitor->loadUserLogsFromDatabase(id);
//         }, Qt::QueuedConnection);

//         monitor->showMaximized();
//         this->close();
//     } else {
//         QMessageBox::warning(this, tr("로그인 실패"), tr("아이디 혹은 비밀번호를 잘못 입력했습니다."));
//         // Close application on login failure
//         // QApplication::quit();
//     }
// }

// void MainWindow::onSignupClicked()
// {
//     SignupWindow dlg(this);
//     if (dlg.exec() == QDialog::Accepted) {
//         const QString newId = dlg.userId();
//         const QString newPw = dlg.userPassword();
//         // 카메라 좌표값도 받아오기
//         double cam1x = dlg.userCamera1X();
//         double cam1y = dlg.userCamera1Y();
//         double cam2x = dlg.userCamera2X();
//         double cam2y = dlg.userCamera2Y();
//         double cam3x = dlg.userCamera3X();
//         double cam3y = dlg.userCamera3Y();
//         if (m_dbManager.registerUser(newId, newPw, cam1x, cam1y, cam2x, cam2y, cam3x, cam3y)) {
//             QMessageBox::information(this, tr("회원가입"), tr("회원가입에 성공했습니다."));
//         } else {
//             QMessageBox::warning(this, tr("회원가입 실패"), tr("이미 존재하는 아이디입니다."));
//         }
//     }
// }




// void MainWindow::publishCameraCoordinates(const QString &userId)
// {
//     DatabaseManager::CameraCoordinates coords;
//     if (m_dbManager.getCameraCoordinates(userId, coords)) {
//         QJsonArray camArray;
//         camArray.append(QJsonObject{{"x", coords.cam1x}, {"y", coords.cam1y}});
//         camArray.append(QJsonObject{{"x", coords.cam2x}, {"y", coords.cam2y}});
//         camArray.append(QJsonObject{{"x", coords.cam3x}, {"y", coords.cam3y}});
//         qDebug() << "[publishCameraCoordinates] Loaded cam1x:" << coords.cam1x;
//         qDebug() << "[publishCameraCoordinates] Loaded cam2x:" << coords.cam2x;
//         qDebug() << "[publishCameraCoordinates] Loaded cam3x:" << coords.cam3x;

//         QJsonObject jsonObj;
//         jsonObj["cameras"] = camArray;

//         QJsonDocument jsonDoc(jsonObj);
//         QByteArray payload = jsonDoc.toJson(QJsonDocument::Compact);

//         QString topic = QString("qt/data/exits");
//         qDebug() << "MQTT 메시지 발행";
//         if (m_mqttClient) {
//             if (!m_mqttClient->publishMessage(topic, payload)) {
//                 qWarning() << "MQTT 메시지 발행 실패";
//             }
//         }
//     }
//     else {
//         qWarning() << "사용자 카메라 좌표 조회 실패";
//     }
// }

// // #include "mainwindow.h"
// // #include "signupwindow.h"
// // #include "monitorwindow.h"
// // #include <QMessageBox>
// // #include <QFormLayout>
// // #include <QHBoxLayout>
// // #include <QApplication>
// // #include <QJsonObject>
// // #include <QJsonArray>
// // #include <QJsonDocument>

// // MainWindow::MainWindow(QWidget *parent)
// //     : QMainWindow(parent)
// //     , m_dbManager("users.db")
// //     , idLineEdit(new QLineEdit(this))
// //     , passwordLineEdit(new QLineEdit(this))
// //     , loginButton(new QPushButton(tr("로그인"), this))
// //     , signupButton(new QPushButton(tr("회원가입"), this))
// //     , m_mqttClient(new Mqtt(this))
// // {
// //     setWindowTitle(tr("로그인"));
// //     setFixedSize(300, 150);

// //     // 비밀번호는 마스킹
// //     passwordLineEdit->setEchoMode(QLineEdit::Password);

// //     // placeholder
// //     idLineEdit->setPlaceholderText(tr("아이디 입력"));
// //     passwordLineEdit->setPlaceholderText(tr("비밀번호 입력"));

// //     // Form 레이아웃에 ID/PW 필드 추가
// //     auto *formLayout = new QFormLayout;
// //     formLayout->addRow(tr("아이디:"),      idLineEdit);
// //     formLayout->addRow(tr("비밀번호:"),  passwordLineEdit);

// //     // 버튼 레이아웃
// //     auto *buttonLayout = new QHBoxLayout;
// //     buttonLayout->addStretch();
// //     buttonLayout->addWidget(loginButton);
// //     buttonLayout->addWidget(signupButton);

// //     // 전체 레이아웃
// //     auto *mainLayout = new QVBoxLayout;
// //     mainLayout->addLayout(formLayout);
// //     mainLayout->addLayout(buttonLayout);

// //     // 중앙 위젯 설정
// //     QWidget *central = new QWidget(this);
// //     central->setLayout(mainLayout);
// //     setCentralWidget(central);

// //     // 시그널 연결
// //     connect(loginButton,  &QPushButton::clicked, this, &MainWindow::onLoginClicked);
// //     connect(signupButton, &QPushButton::clicked, this, &MainWindow::onSignupClicked);
// // }

// // void MainWindow::onLoginClicked()
// // {
// //     const QString id = idLineEdit->text().trimmed();
// //     const QString pw = passwordLineEdit->text();

// //     if (id.isEmpty() || pw.isEmpty()) {
// //         QMessageBox::warning(this, tr("입력 오류"), tr("아이디와 비밀번호를 모두 입력하세요."));
// //         return;
// //     }

// //     if (m_dbManager.loginUser(id, pw)) {
// //         QMessageBox::information(this, tr("로그인"), tr("로그인에 성공했습니다."));

// //         // 로그인 성공 후 mqtt로 바로 메시지 발행 시도
// //         if (m_mqttClient->state() == QMqttClient::Connected) {
// //             publishCameraCoordinates(id);
// //         } else {
// //             // 연결 준비 중이라면: connected 시그널에서 발행하도록 연결
// //             connect(m_mqttClient, &Mqtt::connected, this, [this, id]() {
// //                 publishCameraCoordinates(id);
// //             }, Qt::UniqueConnection);
// //         }

// //         // Open MonitorWindow with integrated RTSP streaming
// //         auto *monitor = new MonitorWindow();
// //         monitor->setCurrentUserId(id); // Pass ID for logout/delete

// //         // 로그인 후 로그 불러오기
// //         QMetaObject::invokeMethod(monitor, [monitor, id]() {
// //             monitor->loadUserLogsFromDatabase(id);
// //         }, Qt::QueuedConnection);

// //         monitor->showMaximized();
// //         this->close();
// //     } else {
// //         QMessageBox::warning(this, tr("로그인 실패"), tr("아이디 혹은 비밀번호를 잘못 입력했습니다."));
// //         // Close application on login failure
// //         // QApplication::quit();
// //     }
// // }

// // void MainWindow::onSignupClicked()
// // {
// //     SignupWindow dlg(this);
// //     if (dlg.exec() == QDialog::Accepted) {
// //         const QString newId = dlg.userId();
// //         const QString newPw = dlg.userPassword();
// //         // 카메라 좌표값도 받아오기
// //         double cam1x = dlg.userCamera1X();
// //         double cam1y = dlg.userCamera1Y();
// //         double cam2x = dlg.userCamera2X();
// //         double cam2y = dlg.userCamera2Y();
// //         double cam3x = dlg.userCamera3X();
// //         double cam3y = dlg.userCamera3Y();
// //         if (m_dbManager.registerUser(newId, newPw, cam1x, cam1y, cam2x, cam2y, cam3x, cam3y)) {
// //             QMessageBox::information(this, tr("회원가입"), tr("회원가입에 성공했습니다."));
// //         } else {
// //             QMessageBox::warning(this, tr("회원가입 실패"), tr("이미 존재하는 아이디입니다."));
// //         }
// //         // if (m_dbManager.registerUser(newId, newPw)) {
// //         //     QMessageBox::information(this, tr("회원가입"), tr("회원가입에 성공했습니다."));
// //         // } else {
// //         //     QMessageBox::warning(this, tr("회원가입 실패"), tr("이미 존재하는 아이디입니다."));
// //         // }
// //     }
// // }

// // void MainWindow::publishCameraCoordinates(const QString &userId)
// // {
// //     DatabaseManager::CameraCoordinates coords;
// //     if (m_dbManager.getCameraCoordinates(userId, coords)) {
// //         QJsonArray camArray;
// //         camArray.append(QJsonObject{{"x", coords.cam1x}, {"y", coords.cam1y}});
// //         camArray.append(QJsonObject{{"x", coords.cam2x}, {"y", coords.cam2y}});
// //         camArray.append(QJsonObject{{"x", coords.cam3x}, {"y", coords.cam3y}});

// //         QJsonObject jsonObj;
// //         jsonObj["cameras"] = camArray;

// //         QJsonDocument jsonDoc(jsonObj);
// //         QByteArray payload = jsonDoc.toJson(QJsonDocument::Compact);

// //         QString topic = QString("qt/data/exits");
// //         qDebug() << "MQTT 메시지 발행";
// //         if (m_mqttClient) {
// //             if (!m_mqttClient->publishMessage(topic, payload)) {
// //                 qWarning() << "MQTT 메시지 발행 실패";
// //             }
// //         }
// //     }
// //     else {
// //         qWarning() << "사용자 카메라 좌표 조회 실패";
// //     }
// // }
// #include "mainwindow.h"
// #include "mqtt.h"
// #include "signupwindow.h"
// #include "monitorwindow.h"
// #include <QMessageBox>
// #include <QFormLayout>
// #include <QHBoxLayout>
// #include <QApplication>
// #include <QJsonObject>
// #include <QJsonArray>
// #include <QJsonDocument>
// #include <QLabel>
// #include <QPixmap>

// MainWindow::MainWindow(QWidget *parent)
//     : QMainWindow(parent)
//     , m_dbManager("users.db")
//     , idLineEdit(new QLineEdit(this))
//     , passwordLineEdit(new QLineEdit(this))
//     , loginButton(new QPushButton(tr("로그인"), this))
//     , signupButton(new QPushButton(tr("회원가입"), this))
//     , m_mqttClient(new Mqtt(this))
//     , loginFrame(new QFrame(this))  //로그인 창 수정 - jjh
// {
//     setWindowTitle(tr("로그인"));
//     setFixedSize(880, 540);  // 창 너비 조정

//     // --- 입력 필드 생성 및 설정 ---
//     idLineEdit = new QLineEdit;
//     passwordLineEdit = new QLineEdit;
//     loginButton = new QPushButton("로그인");
//     signupButton = new QPushButton("회원가입");
//     loginFrame = new QFrame;

//     passwordLineEdit->setEchoMode(QLineEdit::Password);
//     idLineEdit->setPlaceholderText(tr("아이디"));
//     passwordLineEdit->setPlaceholderText(tr("비밀번호"));

//     QString lineEditStyle =
//         "QLineEdit {"
//         "    background-color: #E8E8E8;"
//         "    border: none;"
//         "    border-radius: 4px;"
//         "    padding: 8px 10px;"
//         "    color: #333333;"
//         "    font-size: 12px;"
//         "    font-weight: normal;"
//         "}"
//         "QLineEdit:focus {"
//         "    background-color: #DDDDDD;"
//         "}";

//     idLineEdit->setStyleSheet(lineEditStyle);
//     passwordLineEdit->setStyleSheet(lineEditStyle);

//     QString buttonStyle =
//         "QPushButton {"
//         "    background-color: #E8E8E8;"
//         "    border: none;"
//         "    border-radius: 4px;"
//         "    padding: 8px 15px;"
//         "    color: #333333;"
//         "    font-weight: bold;"
//         "    font-size: 12px;"
//         "}"
//         "QPushButton:hover {"
//         "    background-color: #DDDDDD;"
//         "}"
//         "QPushButton:pressed {"
//         "    background-color: #CCCCCC;"
//         "}";

//     loginButton->setStyleSheet(buttonStyle);
//     signupButton->setStyleSheet(buttonStyle);

//     // --- 로그인 프레임 내부 레이아웃 ---
//     auto *inputLayout = new QVBoxLayout;
//     inputLayout->setSpacing(10);
//     inputLayout->addWidget(idLineEdit);
//     inputLayout->addWidget(passwordLineEdit);

//     auto *buttonLayout = new QHBoxLayout;
//     buttonLayout->setSpacing(10);
//     buttonLayout->addWidget(loginButton);
//     buttonLayout->addWidget(signupButton);

//     auto *loginLayout = new QVBoxLayout(loginFrame);
//     loginLayout->setSpacing(15);
//     loginLayout->setContentsMargins(0, 0, 0, 0);
//     loginLayout->addLayout(inputLayout);
//     loginLayout->addLayout(buttonLayout);

//     loginFrame->setFixedWidth(240);
//     loginFrame->setStyleSheet("QFrame { background-color: transparent; }");

//     // --- 왼쪽 패널 (주황 배경) ---
//     QWidget *leftPanel = new QWidget;
//     leftPanel->setFixedWidth(300);
//     leftPanel->setStyleSheet("background-color: #f37321;");
//     QVBoxLayout *leftLayout = new QVBoxLayout(leftPanel);
//     leftLayout->setAlignment(Qt::AlignCenter);
//     leftLayout->addWidget(loginFrame);

//     // --- 오른쪽 이미지 삽입 ---
//     QLabel *imageLabel = new QLabel;
//     imageLabel->setAlignment(Qt::AlignCenter);
//     imageLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
//     imageLabel->setStyleSheet("background-color: white;");  // 이미지 위젯 배경 흰색

//     QPixmap backgroundImage(":/images/images/login_image.png");  // qrc에 등록된 경로
//     if (!backgroundImage.isNull()) {
//         QPixmap scaledImage = backgroundImage.scaled(
//             560, 480, Qt::KeepAspectRatio, Qt::SmoothTransformation
//             );
//         imageLabel->setPixmap(scaledImage);
//     } else {
//         imageLabel->setText("이미지를 불러올 수 없습니다");
//         imageLabel->setStyleSheet("color: #999999; font-size: 14px;");
//     }

//     // --- 메인 레이아웃 ---
//     QHBoxLayout *mainLayout = new QHBoxLayout;
//     mainLayout->setContentsMargins(0, 0, 0, 0);
//     mainLayout->setSpacing(0);
//     mainLayout->addWidget(leftPanel);
//     mainLayout->addWidget(imageLabel);

//     QWidget *central = new QWidget(this);
//     central->setLayout(mainLayout);
//     central->setStyleSheet("background-color: white;");  // ✨ 배경 흰색 설정
//     setCentralWidget(central);

//     // --- 시그널 연결 ---
//     connect(loginButton, &QPushButton::clicked, this, &MainWindow::onLoginClicked);
//     connect(signupButton, &QPushButton::clicked, this, &MainWindow::onSignupClicked);
// }


// void MainWindow::onLoginClicked()
// {
//     const QString id = idLineEdit->text().trimmed();
//     const QString pw = passwordLineEdit->text();

//     if (id.isEmpty() || pw.isEmpty()) {
//         QMessageBox::warning(this, tr("입력 오류"), tr("아이디와 비밀번호를 모두 입력하세요."));
//         return;
//     }

//     if (m_dbManager.loginUser(id, pw)) {
//         QMessageBox::information(this, tr("로그인"), tr("로그인에 성공했습니다."));

//         // 로그인 성공 후 mqtt로 바로 메시지 발행 시도
//         if (m_mqttClient->state() == QMqttClient::Connected) {
//             publishCameraCoordinates(id);
//         } else {
//             // 연결 준비 중이라면: connected 시그널에서 발행하도록 연결
//             connect(m_mqttClient, &Mqtt::connected, this, [this, id]() {
//                 publishCameraCoordinates(id);
//             }, Qt::UniqueConnection);
//         }

//         // Open MonitorWindow with integrated RTSP streaming
//         auto *monitor = new MonitorWindow(id);
//         monitor->setCurrentUserId(id); // Pass ID for logout/delete

//         // 로그인 후 로그 불러오기
//         QMetaObject::invokeMethod(monitor, [monitor, id]() {
//             monitor->loadUserLogsFromDatabase(id);
//         }, Qt::QueuedConnection);

//         monitor->showMaximized();
//         this->close();
//     } else {
//         QMessageBox::warning(this, tr("로그인 실패"), tr("아이디 혹은 비밀번호를 잘못 입력했습니다."));
//         // Close application on login failure
//         // QApplication::quit();
//     }
// }

// void MainWindow::onSignupClicked()
// {
//     SignupWindow dlg(this);
//     if (dlg.exec() == QDialog::Accepted) {
//         const QString newId = dlg.userId();
//         const QString newPw = dlg.userPassword();
//         // 카메라 좌표값도 받아오기
//         double cam1x = dlg.userCamera1X();
//         double cam1y = dlg.userCamera1Y();
//         double cam2x = dlg.userCamera2X();
//         double cam2y = dlg.userCamera2Y();
//         double cam3x = dlg.userCamera3X();
//         double cam3y = dlg.userCamera3Y();
//         if (m_dbManager.registerUser(newId, newPw, cam1x, cam1y, cam2x, cam2y, cam3x, cam3y)) {
//             QMessageBox::information(this, tr("회원가입"), tr("회원가입에 성공했습니다."));
//         } else {
//             QMessageBox::warning(this, tr("회원가입 실패"), tr("이미 존재하는 아이디입니다."));
//         }
//     }
// }




// void MainWindow::publishCameraCoordinates(const QString &userId)
// {
//     DatabaseManager::CameraCoordinates coords;
//     if (m_dbManager.getCameraCoordinates(userId, coords)) {
//         QJsonArray camArray;
//         camArray.append(QJsonObject{{"x", coords.cam1x}, {"y", coords.cam1y}});
//         camArray.append(QJsonObject{{"x", coords.cam2x}, {"y", coords.cam2y}});
//         camArray.append(QJsonObject{{"x", coords.cam3x}, {"y", coords.cam3y}});
//         qDebug() << "[publishCameraCoordinates] Loaded cam1x:" << coords.cam1x;
//         qDebug() << "[publishCameraCoordinates] Loaded cam2x:" << coords.cam2x;
//         qDebug() << "[publishCameraCoordinates] Loaded cam3x:" << coords.cam3x;

//         QJsonObject jsonObj;
//         jsonObj["cameras"] = camArray;

//         QJsonDocument jsonDoc(jsonObj);
//         QByteArray payload = jsonDoc.toJson(QJsonDocument::Compact);

//         QString topic = QString("qt/data/exits");
//         qDebug() << "MQTT 메시지 발행";
//         if (m_mqttClient) {
//             if (!m_mqttClient->publishMessage(topic, payload)) {
//                 qWarning() << "MQTT 메시지 발행 실패";
//             }
//         }
//     }
//     else {
//         qWarning() << "사용자 카메라 좌표 조회 실패";
//     }
// }
#include "mainwindow.h"
#include "mqtt.h"
#include "monitorwindow.h"
#include <QMessageBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QScrollArea>
#include <QApplication>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonDocument>
#include <QLabel>
#include <QPixmap>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , m_dbManager("users.db")
    , m_mqttClient(new Mqtt(this))
    , tabWidget(new QTabWidget(this))
{
    setWindowTitle(tr("PIT-STOP"));
    setFixedSize(500, 700);

    // 메인 윈도우 다크 테마 설정 (MonitorWindow와 일치)
    setStyleSheet(R"(
        QMainWindow {
            background-color: #1A1E2A;
        }
        QTabWidget {
            background-color: #1A1E2A;
        }
        QTabWidget::pane {
            border: none;
            background-color: #1B2533;
            border-radius: 12px;
        }
        QTabBar {
            background-color: #1B2533;
        }
        QTabBar::tab {
            background: transparent;
            color: #999999;
            padding: 10px 32px;
            font-size: 13px;
            font-weight: 500;
            border: 1px solid transparent;
            border-radius: 12px;
            margin-right: 6px;
            min-width: 94px;
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
        QLabel {
            color: #ffffff;
            font-size: 14px;
        }
        QLineEdit {
            background-color: #334155;
            border: 1px solid #475569;
            border-radius: 8px;
            padding: 10px;
            color: #ffffff;
            font-size: 14px;
        }
        QLineEdit:focus {
            border: 2px solid #3b82f6;
        }
        QPushButton {
            background-color: #3b82f6;
            border: none;
            border-radius: 8px;
            color: white;
            font-size: 14px;
            font-weight: bold;
            padding: 12px 24px;
        }
        QPushButton:hover {
            background-color: #2563eb;
        }
        QPushButton:pressed {
            background-color: #1d4ed8;
        }
    )");

    // 탭 상단을 꽉 채우도록 설정
    tabWidget->tabBar()->setExpanding(true);

    // 탭 생성
    tabWidget->addTab(createLoginTab(), "로그인");
    tabWidget->addTab(createSignupTab(), "회원가입");
    tabWidget->addTab(createDeleteAccountTab(), "회원탈퇴");

    setCentralWidget(tabWidget);

    // 시그널 연결
    connect(loginButton, &QPushButton::clicked, this, &MainWindow::onLoginClicked);
    connect(signupButton, &QPushButton::clicked, this, &MainWindow::onSignupClicked);
    connect(deleteAccountButton, &QPushButton::clicked, this, &MainWindow::onDeleteAccountClicked);
}

QWidget* MainWindow::createLoginTab()
{
    QWidget *loginTab = new QWidget;

    // 입력 필드 초기화
    loginIdLineEdit = new QLineEdit;
    loginPasswordLineEdit = new QLineEdit;
    loginButton = new QPushButton(tr("로그인"));

    // 입력 필드 스타일
    QString lineEditStyle =
        "QLineEdit {"
        "    background-color: #2a2a3e;"
        "    border: 1px solid #3a3a4e;"
        "    border-radius: 8px;"
        "    padding: 12px 16px;"
        "    color: white;"
        "    font-size: 14px;"
        "    margin: 8px 0px;"
        "}"
        "QLineEdit:focus {"
        "    border: 1px solid #4a9eff;"
        "    background-color: #353550;"
        "}"
        "QLineEdit::placeholder {"
        "    color: #8a8a9a;"
        "}";

    loginIdLineEdit->setStyleSheet(lineEditStyle);
    loginPasswordLineEdit->setStyleSheet(lineEditStyle);
    loginPasswordLineEdit->setEchoMode(QLineEdit::Password);

    loginIdLineEdit->setPlaceholderText("아이디");
    loginPasswordLineEdit->setPlaceholderText("비밀번호");

    // 로그인 버튼 스타일
    QString loginButtonStyle =
        "QPushButton {"
        "    background: qlineargradient(x1:0, y1:0, x2:1, y2:0, "
        "                stop:0 #4a9eff, stop:1 #0f7cff);"
        "    border: none;"
        "    border-radius: 8px;"
        "    padding: 12px;"
        "    color: white;"
        "    font-weight: bold;"
        "    font-size: 14px;"
        "    margin: 16px 0px 8px 0px;"
        "}"
        "QPushButton:hover {"
        "    background: qlineargradient(x1:0, y1:0, x2:1, y2:0, "
        "                stop:0 #5aa9ff, stop:1 #1f8cff);"
        "}"
        "QPushButton:pressed {"
        "    background: qlineargradient(x1:0, y1:0, x2:1, y2:0, "
        "                stop:0 #3a8eef, stop:1 #0a6cef);"
        "}";

    loginButton->setStyleSheet(loginButtonStyle);

    // 레이아웃
    QVBoxLayout *layout = new QVBoxLayout(loginTab);
    layout->setSpacing(16);
    layout->setContentsMargins(40, 40, 40, 40);

    // 위쪽 여백 추가로 폼을 아래로 내림
    layout->addStretch(1);

    // 시스템 이미지 추가
    QLabel *systemImageLabel = new QLabel;
    systemImageLabel->setAlignment(Qt::AlignCenter);
    QPixmap systemImage(":/new/prefix1/images/System_Name.png");
    if (!systemImage.isNull()) {
        QPixmap scaledSystemImage = systemImage.scaled(
            400, 120, Qt::KeepAspectRatio, Qt::SmoothTransformation
        );
        systemImageLabel->setPixmap(scaledSystemImage);
    } else {
        systemImageLabel->setText("System Logo");
        systemImageLabel->setStyleSheet("color: #999999; font-size: 14px;");
    }

    QLabel *titleLabel = new QLabel("PIT-STOP");
    titleLabel->setAlignment(Qt::AlignCenter);
    titleLabel->setStyleSheet(
        "QLabel {"
        "    color: white;"
        "    font-size: 36px;"
        "    font-weight: bold;"
        "    margin: 10px 0px;"
        "}"
        );

    layout->addWidget(systemImageLabel);
    layout->addWidget(titleLabel);
    layout->addSpacing(20);
    layout->addWidget(loginIdLineEdit);
    layout->addWidget(loginPasswordLineEdit);
    layout->addWidget(loginButton);
    layout->addStretch(2);

    return loginTab;
}

QWidget* MainWindow::createSignupTab()
{
    QWidget *signupTab = new QWidget;

    // 스크롤 영역 생성
    QScrollArea *scrollArea = new QScrollArea;
    scrollArea->setWidgetResizable(true);
    scrollArea->setStyleSheet(
        "QScrollArea {"
        "    background-color: transparent;"
        "    border: none;"
        "}"
        "QScrollBar:vertical {"
        "    background-color: #2a2a3e;"
        "    width: 8px;"
        "    border-radius: 4px;"
        "}"
        "QScrollBar::handle:vertical {"
        "    background-color: #4a4a5e;"
        "    border-radius: 4px;"
        "    min-height: 20px;"
        "}"
        "QScrollBar::handle:vertical:hover {"
        "    background-color: #5a5a6e;"
        "}"
        );

    QWidget *scrollWidget = new QWidget;

    // 입력 필드 초기화
    signupIdLineEdit = new QLineEdit;
    signupPasswordLineEdit = new QLineEdit;
    signupPasswordConfirmLineEdit = new QLineEdit;
    camera1XLineEdit = new QLineEdit;
    camera1YLineEdit = new QLineEdit;
    camera2XLineEdit = new QLineEdit;
    camera2YLineEdit = new QLineEdit;
    camera3XLineEdit = new QLineEdit;
    camera3YLineEdit = new QLineEdit;
    signupButton = new QPushButton(tr("회원가입"));

    // 입력 필드 스타일
    QString lineEditStyle =
        "QLineEdit {"
        "    background-color: #2a2a3e;"
        "    border: 1px solid #3a3a4e;"
        "    border-radius: 8px;"
        "    padding: 10px 12px;"
        "    color: white;"
        "    font-size: 13px;"
        "    margin: 1px 0px;"
        "}"
        "QLineEdit:focus {"
        "    border: 1px solid #4a9eff;"
        "    background-color: #353550;"
        "}"
        "QLineEdit::placeholder {"
        "    color: #8a8a9a;"
        "}";

    QString labelStyle =
        "QLabel {"
        "    color: #cccccc;"
        "    font-size: 13px;"
        "    font-weight: 500;"
        "    margin: 2px 0px 1px 0px;"
        "}";

    // 모든 입력 필드에 스타일 적용
    signupIdLineEdit->setStyleSheet(lineEditStyle);
    signupPasswordLineEdit->setStyleSheet(lineEditStyle);
    signupPasswordConfirmLineEdit->setStyleSheet(lineEditStyle);
    camera1XLineEdit->setStyleSheet(lineEditStyle);
    camera1YLineEdit->setStyleSheet(lineEditStyle);
    camera2XLineEdit->setStyleSheet(lineEditStyle);
    camera2YLineEdit->setStyleSheet(lineEditStyle);
    camera3XLineEdit->setStyleSheet(lineEditStyle);
    camera3YLineEdit->setStyleSheet(lineEditStyle);

    // 패스워드 마스킹
    signupPasswordLineEdit->setEchoMode(QLineEdit::Password);
    signupPasswordConfirmLineEdit->setEchoMode(QLineEdit::Password);

    // placeholder 설정
    signupIdLineEdit->setPlaceholderText("사용할 아이디를 입력하세요");
    signupPasswordLineEdit->setPlaceholderText("비밀번호를 입력하세요");
    signupPasswordConfirmLineEdit->setPlaceholderText("비밀번호를 다시 입력하세요");
    camera1XLineEdit->setPlaceholderText("X 좌표");
    camera1YLineEdit->setPlaceholderText("Y 좌표");
    camera2XLineEdit->setPlaceholderText("X 좌표");
    camera2YLineEdit->setPlaceholderText("Y 좌표");
    camera3XLineEdit->setPlaceholderText("X 좌표");
    camera3YLineEdit->setPlaceholderText("Y 좌표");

    // 회원가입 버튼 스타일
    QString signupButtonStyle =
        "QPushButton {"
        "    background: qlineargradient(x1:0, y1:0, x2:1, y2:0, "
        "                stop:0 #4a9eff, stop:1 #0f7cff);"
        "    border: none;"
        "    border-radius: 8px;"
        "    padding: 12px;"
        "    color: white;"
        "    font-weight: bold;"
        "    font-size: 14px;"
        "    margin: 6px 4px 4px 4px;"
        "}"
        "QPushButton:hover {"
        "    background: qlineargradient(x1:0, y1:0, x2:1, y2:0, "
        "                stop:0 #5aa9ff, stop:1 #1f8cff);"
        "}"
        "QPushButton:pressed {"
        "    background: qlineargradient(x1:0, y1:0, x2:1, y2:0, "
        "                stop:0 #3a8eef, stop:1 #0a6cef);"
        "}";

    signupButton->setStyleSheet(signupButtonStyle);

    // 레이아웃
    QVBoxLayout *formLayout = new QVBoxLayout(scrollWidget);
    formLayout->setSpacing(4);
    formLayout->setContentsMargins(15, 15, 15, 10);

    QLabel *titleLabel = new QLabel("회원가입");
    titleLabel->setAlignment(Qt::AlignCenter);
    titleLabel->setStyleSheet(
        "QLabel {"
        "    color: white;"
        "    font-size: 24px;"
        "    font-weight: bold;"
        "    margin: 5px 0px;"
        "}"
        );
    formLayout->addWidget(titleLabel);

    // 계정 정보 섹션
    QLabel *accountLabel = new QLabel("계정 정보");
    accountLabel->setStyleSheet(labelStyle + "font-size: 15px; font-weight: bold; color: #4a9eff; margin-top: 0px;");
    formLayout->addWidget(accountLabel);

    QLabel *idLabel = new QLabel("아이디");
    idLabel->setStyleSheet(labelStyle);
    formLayout->addWidget(idLabel);
    formLayout->addWidget(signupIdLineEdit);

    QLabel *pwLabel = new QLabel("비밀번호");
    pwLabel->setStyleSheet(labelStyle);
    formLayout->addWidget(pwLabel);
    formLayout->addWidget(signupPasswordLineEdit);

    QLabel *pwConfirmLabel = new QLabel("비밀번호 확인");
    pwConfirmLabel->setStyleSheet(labelStyle);
    formLayout->addWidget(pwConfirmLabel);
    formLayout->addWidget(signupPasswordConfirmLineEdit);

    // 카메라 설정 섹션
    QLabel *cameraLabel = new QLabel("카메라 좌표 설정");
    cameraLabel->setStyleSheet(labelStyle + "font-size: 15px; font-weight: bold; color: #4a9eff; margin-top: 5px;");
    formLayout->addWidget(cameraLabel);

    // 카메라 1
    QLabel *cam1Label = new QLabel("카메라 1");
    cam1Label->setStyleSheet(labelStyle + "margin-top: 4px;");
    formLayout->addWidget(cam1Label);

    QHBoxLayout *cam1Layout = new QHBoxLayout;
    cam1Layout->addWidget(camera1XLineEdit);
    cam1Layout->addWidget(camera1YLineEdit);
    formLayout->addLayout(cam1Layout);

    // 카메라 2
    QLabel *cam2Label = new QLabel("카메라 2");
    cam2Label->setStyleSheet(labelStyle);
    formLayout->addWidget(cam2Label);

    QHBoxLayout *cam2Layout = new QHBoxLayout;
    cam2Layout->addWidget(camera2XLineEdit);
    cam2Layout->addWidget(camera2YLineEdit);
    formLayout->addLayout(cam2Layout);

    // 카메라 3
    QLabel *cam3Label = new QLabel("카메라 3");
    cam3Label->setStyleSheet(labelStyle);
    formLayout->addWidget(cam3Label);

    QHBoxLayout *cam3Layout = new QHBoxLayout;
    cam3Layout->addWidget(camera3XLineEdit);
    cam3Layout->addWidget(camera3YLineEdit);
    formLayout->addLayout(cam3Layout);

    formLayout->addWidget(signupButton);

    scrollArea->setWidget(scrollWidget);

    QVBoxLayout *tabLayout = new QVBoxLayout(signupTab);
    tabLayout->setContentsMargins(0, 0, 0, 0);
    tabLayout->addWidget(scrollArea);

    return signupTab;
}

QWidget* MainWindow::createDeleteAccountTab()
{
    QWidget *deleteTab = new QWidget;

    // 입력 필드 초기화
    deleteIdLineEdit = new QLineEdit;
    deletePasswordLineEdit = new QLineEdit;
    deleteAccountButton = new QPushButton(tr("회원탈퇴"));

    // 입력 필드 스타일
    QString lineEditStyle =
        "QLineEdit {"
        "    background-color: #2a2a3e;"
        "    border: 1px solid #3a3a4e;"
        "    border-radius: 8px;"
        "    padding: 12px 16px;"
        "    color: white;"
        "    font-size: 14px;"
        "    margin: 8px 0px;"
        "}"
        "QLineEdit:focus {"
        "    border: 1px solid #ff4a4a;"
        "    background-color: #353550;"
        "}"
        "QLineEdit::placeholder {"
        "    color: #8a8a9a;"
        "}";

    deleteIdLineEdit->setStyleSheet(lineEditStyle);
    deletePasswordLineEdit->setStyleSheet(lineEditStyle);
    deletePasswordLineEdit->setEchoMode(QLineEdit::Password);

    deleteIdLineEdit->setPlaceholderText("탈퇴할 아이디");
    deletePasswordLineEdit->setPlaceholderText("비밀번호");

    // 탈퇴 버튼 스타일 (빨간색)
    QString deleteButtonStyle =
        "QPushButton {"
        "    background: qlineargradient(x1:0, y1:0, x2:1, y2:0, "
        "                stop:0 #ff4a4a, stop:1 #cc0000);"
        "    border: none;"
        "    border-radius: 8px;"
        "    padding: 12px;"
        "    color: white;"
        "    font-weight: bold;"
        "    font-size: 14px;"
        "    margin: 16px 0px 8px 0px;"
        "}"
        "QPushButton:hover {"
        "    background: qlineargradient(x1:0, y1:0, x2:1, y2:0, "
        "                stop:0 #ff6a6a, stop:1 #dd2020);"
        "}"
        "QPushButton:pressed {"
        "    background: qlineargradient(x1:0, y1:0, x2:1, y2:0, "
        "                stop:0 #dd2020, stop:1 #aa0000);"
        "}";

    deleteAccountButton->setStyleSheet(deleteButtonStyle);

    // 레이아웃
    QVBoxLayout *layout = new QVBoxLayout(deleteTab);
    layout->setSpacing(16);
    layout->setContentsMargins(40, 40, 40, 40);

    QLabel *titleLabel = new QLabel("회원탈퇴");
    titleLabel->setAlignment(Qt::AlignCenter);
    titleLabel->setStyleSheet(
        "QLabel {"
        "    color: white;"
        "    font-size: 24px;"
        "    font-weight: bold;"
        "    margin: 20px 0px;"
        "}"
        );

    QLabel *warningLabel = new QLabel("주의: 회원탈퇴 시 모든 데이터가 삭제됩니다.");
    warningLabel->setAlignment(Qt::AlignCenter);
    warningLabel->setStyleSheet(
        "QLabel {"
        "    color: #ff4a4a;"
        "    font-size: 14px;"
        "    font-weight: bold;"
        "    margin: 10px 0px;"
        "    background-color: rgba(255, 74, 74, 0.1);"
        "    padding: 10px;"
        "    border-radius: 8px;"
        "}"
        );

    layout->addWidget(titleLabel);
    layout->addWidget(warningLabel);
    layout->addSpacing(20);
    layout->addWidget(deleteIdLineEdit);
    layout->addWidget(deletePasswordLineEdit);
    layout->addWidget(deleteAccountButton);
    layout->addStretch();

    return deleteTab;
}

void MainWindow::onLoginClicked()
{
    const QString id = loginIdLineEdit->text().trimmed();
    const QString pw = loginPasswordLineEdit->text();

    if (id.isEmpty() || pw.isEmpty()) {
        // 다크 테마에 맞는 메시지박스 스타일
        QMessageBox msgBox(this);
        msgBox.setWindowTitle("입력 오류");
        msgBox.setText("아이디와 비밀번호를 모두 입력하세요.");
        msgBox.setIcon(QMessageBox::Warning);
        msgBox.setStyleSheet(
            "QMessageBox {"
            "    background-color: #2a2a3e;"
            "    color: white;"
            "}"
            "QMessageBox QPushButton {"
            "    background-color: #4a9eff;"
            "    border: none;"
            "    border-radius: 4px;"
            "    padding: 8px 16px;"
            "    color: white;"
            "    font-weight: bold;"
            "}"
            "QMessageBox QPushButton:hover {"
            "    background-color: #5aa9ff;"
            "}"
            );
        msgBox.exec();
        return;
    }

    if (m_dbManager.loginUser(id, pw)) {
        QMessageBox msgBox(this);
        msgBox.setWindowTitle("로그인");
        msgBox.setText("로그인에 성공했습니다.");
        msgBox.setIcon(QMessageBox::Information);
        msgBox.setStyleSheet(
            "QMessageBox {"
            "    background-color: #2a2a3e;"
            "    color: white;"
            "}"
            "QMessageBox QPushButton {"
            "    background-color: #4a9eff;"
            "    border: none;"
            "    border-radius: 4px;"
            "    padding: 8px 16px;"
            "    color: white;"
            "    font-weight: bold;"
            "}"
            );
        msgBox.exec();

        // 로그인 성공 후 mqtt로 바로 메시지 발행 시도
        if (m_mqttClient->state() == QMqttClient::Connected) {
            publishCameraCoordinates(id);
        } else {
            // 연결 준비 중이라면: connected 시그널에서 발행하도록 연결
            connect(m_mqttClient, &Mqtt::connected, this, [this, id]() {
                publishCameraCoordinates(id);
            }, Qt::UniqueConnection);
        }

        // Open MonitorWindow with integrated RTSP streaming
        auto *monitor = new MonitorWindow(id);
        monitor->setCurrentUserId(id); // Pass ID for logout/delete

        // 로그인 후 로그 불러오기
        QMetaObject::invokeMethod(monitor, [monitor, id]() {
            monitor->loadUserLogsFromDatabase(id);
        }, Qt::QueuedConnection);

        monitor->showMaximized();
        this->close();
    } else {
        QMessageBox msgBox(this);
        msgBox.setWindowTitle("로그인 실패");
        msgBox.setText("아이디 혹은 비밀번호를 잘못 입력했습니다.");
        msgBox.setIcon(QMessageBox::Warning);
        msgBox.setStyleSheet(
            "QMessageBox {"
            "    background-color: #2a2a3e;"
            "    color: white;"
            "}"
            "QMessageBox QPushButton {"
            "    background-color: #ff4a4a;"
            "    border: none;"
            "    border-radius: 4px;"
            "    padding: 8px 16px;"
            "    color: white;"
            "    font-weight: bold;"
            "}"
            );
        msgBox.exec();
    }
}

void MainWindow::onSignupClicked()
{
    const QString newId = signupIdLineEdit->text().trimmed();
    const QString newPw = signupPasswordLineEdit->text();
    const QString pwConfirm = signupPasswordConfirmLineEdit->text();

    bool ok1x, ok1y, ok2x, ok2y, ok3x, ok3y;
    double cam1x = camera1XLineEdit->text().toDouble(&ok1x);
    double cam1y = camera1YLineEdit->text().toDouble(&ok1y);
    double cam2x = camera2XLineEdit->text().toDouble(&ok2x);
    double cam2y = camera2YLineEdit->text().toDouble(&ok2y);
    double cam3x = camera3XLineEdit->text().toDouble(&ok3x);
    double cam3y = camera3YLineEdit->text().toDouble(&ok3y);

    QString messageBoxStyle =
        "QMessageBox {"
        "    background-color: #2a2a3e;"
        "    color: white;"
        "}"
        "QMessageBox QPushButton {"
        "    background-color: #4a9eff;"
        "    border: none;"
        "    border-radius: 4px;"
        "    padding: 8px 16px;"
        "    color: white;"
        "    font-weight: bold;"
        "}"
        "QMessageBox QPushButton:hover {"
        "    background-color: #5aa9ff;"
        "}";

    if (newId.isEmpty() || newPw.isEmpty() || pwConfirm.isEmpty()) {
        QMessageBox msgBox(this);
        msgBox.setWindowTitle("입력 오류");
        msgBox.setText("모든 항목을 입력하세요.");
        msgBox.setIcon(QMessageBox::Warning);
        msgBox.setStyleSheet(messageBoxStyle);
        msgBox.exec();
        return;
    }

    if (newPw != pwConfirm) {
        QMessageBox msgBox(this);
        msgBox.setWindowTitle("비밀번호 불일치");
        msgBox.setText("비밀번호가 일치하지 않습니다.");
        msgBox.setIcon(QMessageBox::Warning);
        msgBox.setStyleSheet(messageBoxStyle);
        msgBox.exec();
        return;
    }

    if (!ok1x || !ok1y || !ok2x || !ok2y || !ok3x || !ok3y) {
        QMessageBox msgBox(this);
        msgBox.setWindowTitle("입력 오류");
        msgBox.setText("카메라 좌표를 모두 정확히 입력하세요.");
        msgBox.setIcon(QMessageBox::Warning);
        msgBox.setStyleSheet(messageBoxStyle);
        msgBox.exec();
        return;
    }

    if (m_dbManager.registerUser(newId, newPw, cam1x, cam1y, cam2x, cam2y, cam3x, cam3y)) {
        QMessageBox msgBox(this);
        msgBox.setWindowTitle("회원가입");
        msgBox.setText("회원가입에 성공했습니다.");
        msgBox.setIcon(QMessageBox::Information);
        msgBox.setStyleSheet(messageBoxStyle);
        msgBox.exec();

        // 입력 필드 초기화
        signupIdLineEdit->clear();
        signupPasswordLineEdit->clear();
        signupPasswordConfirmLineEdit->clear();
        camera1XLineEdit->clear();
        camera1YLineEdit->clear();
        camera2XLineEdit->clear();
        camera2YLineEdit->clear();
        camera3XLineEdit->clear();
        camera3YLineEdit->clear();

        // 로그인 탭으로 이동
        tabWidget->setCurrentIndex(0);
    } else {
        QMessageBox msgBox(this);
        msgBox.setWindowTitle("회원가입 실패");
        msgBox.setText("이미 존재하는 아이디입니다.");
        msgBox.setIcon(QMessageBox::Warning);
        msgBox.setStyleSheet(
            "QMessageBox {"
            "    background-color: #2a2a3e;"
            "    color: white;"
            "}"
            "QMessageBox QPushButton {"
            "    background-color: #ff4a4a;"
            "    border: none;"
            "    border-radius: 4px;"
            "    padding: 8px 16px;"
            "    color: white;"
            "    font-weight: bold;"
            "}"
            );
        msgBox.exec();
    }
}

void MainWindow::onDeleteAccountClicked()
{
    const QString id = deleteIdLineEdit->text().trimmed();
    const QString pw = deletePasswordLineEdit->text();

    QString messageBoxStyle =
        "QMessageBox {"
        "    background-color: #2a2a3e;"
        "    color: white;"
        "}"
        "QMessageBox QPushButton {"
        "    background-color: #ff4a4a;"
        "    border: none;"
        "    border-radius: 4px;"
        "    padding: 8px 16px;"
        "    color: white;"
        "    font-weight: bold;"
        "}"
        "QMessageBox QPushButton:hover {"
        "    background-color: #ff6a6a;"
        "}";

    if (id.isEmpty() || pw.isEmpty()) {
        QMessageBox msgBox(this);
        msgBox.setWindowTitle("입력 오류");
        msgBox.setText("아이디와 비밀번호를 모두 입력하세요.");
        msgBox.setIcon(QMessageBox::Warning);
        msgBox.setStyleSheet(messageBoxStyle);
        msgBox.exec();
        return;
    }

    // 먼저 로그인 검증
    if (!m_dbManager.loginUser(id, pw)) {
        QMessageBox msgBox(this);
        msgBox.setWindowTitle("인증 실패");
        msgBox.setText("아이디 혹은 비밀번호가 올바르지 않습니다.");
        msgBox.setIcon(QMessageBox::Warning);
        msgBox.setStyleSheet(messageBoxStyle);
        msgBox.exec();
        return;
    }

    // 최종 확인
    QMessageBox confirmBox(this);
    confirmBox.setWindowTitle("회원탈퇴 확인");
    confirmBox.setText(QString("정말로 '%1' 계정을 삭제하시겠습니까?\n이 작업은 되돌릴 수 없습니다.").arg(id));
    confirmBox.setIcon(QMessageBox::Warning);
    confirmBox.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
    confirmBox.setDefaultButton(QMessageBox::No);
    confirmBox.setStyleSheet(messageBoxStyle);

    if (confirmBox.exec() == QMessageBox::Yes) {
        if (m_dbManager.deleteUser(id)) {
            QMessageBox msgBox(this);
            msgBox.setWindowTitle("회원탈퇴");
            msgBox.setText("회원탈퇴가 완료되었습니다.");
            msgBox.setIcon(QMessageBox::Information);
            msgBox.setStyleSheet(
                "QMessageBox {"
                "    background-color: #2a2a3e;"
                "    color: white;"
                "}"
                "QMessageBox QPushButton {"
                "    background-color: #4a9eff;"
                "    border: none;"
                "    border-radius: 4px;"
                "    padding: 8px 16px;"
                "    color: white;"
                "    font-weight: bold;"
                "}"
                );
            msgBox.exec();

            // 입력 필드 초기화
            deleteIdLineEdit->clear();
            deletePasswordLineEdit->clear();

            // 로그인 탭으로 이동
            tabWidget->setCurrentIndex(0);
        } else {
            QMessageBox msgBox(this);
            msgBox.setWindowTitle("탈퇴 실패");
            msgBox.setText("회원탈퇴 처리 중 오류가 발생했습니다.");
            msgBox.setIcon(QMessageBox::Critical);
            msgBox.setStyleSheet(messageBoxStyle);
            msgBox.exec();
        }
    }
}

void MainWindow::publishCameraCoordinates(const QString &userId)
{
    DatabaseManager::CameraCoordinates coords;
    if (m_dbManager.getCameraCoordinates(userId, coords)) {
        QJsonArray camArray;
        camArray.append(QJsonObject{{"x", coords.cam1x}, {"y", coords.cam1y}});
        camArray.append(QJsonObject{{"x", coords.cam2x}, {"y", coords.cam2y}});
        camArray.append(QJsonObject{{"x", coords.cam3x}, {"y", coords.cam3y}});

        QJsonObject jsonObj;
        jsonObj["cameras"] = camArray;

        QJsonDocument jsonDoc(jsonObj);
        QByteArray payload = jsonDoc.toJson(QJsonDocument::Compact);

        QString topic = QString("qt/data/exits");
        qDebug() << "MQTT 메시지 발행";
        if (m_mqttClient) {
            if (!m_mqttClient->publishMessage(topic, payload)) {
                qWarning() << "MQTT 메시지 발행 실패";
            }
        }
    }
    else {
        qWarning() << "사용자 카메라 좌표 조회 실패";
    }
}

