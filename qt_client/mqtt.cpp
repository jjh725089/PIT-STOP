#include "mqtt.h"
#include "monitorwindow.h"
#include <QVBoxLayout>
#include <QTextEdit>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSslSocket>
#include <QSslConfiguration>
#include <QSqlError>
#include <QJsonDocument>
#include <QJsonObject>
#include <QDateTime>
#include <QDebug>
#include <QFile>
#include <QMessageBox>
#include <QTextStream>
#include <QDir>
#include <QStandardPaths>

Mqtt::Mqtt(QWidget *parent)
    : QWidget(parent)
    , m_client(nullptr)
{
    qDebug() << "OpenSSL 지원 여부:" << QSslSocket::supportsSsl();
    qDebug() << "OpenSSL 빌드 버전:" << QSslSocket::sslLibraryBuildVersionString();
    qDebug() << "OpenSSL 런타임 버전:" << QSslSocket::sslLibraryVersionString();

    logTextEdit = new QTextEdit(this);
    logTextEdit->setReadOnly(true);

    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->addWidget(logTextEdit);

    // Initialize MQTT client after widget setup is complete
    m_client = new QMqttClient(this);

    // Use unique database connection name to avoid conflicts
    QString connectionName = QString("mqtt_connection_%1").arg(quintptr(this));
    m_db = QSqlDatabase::addDatabase("QSQLITE", connectionName);
    m_db.setDatabaseName("sensor_data.db");
    if (!m_db.open()) {
        logTextEdit->append("DB Error: " + m_db.lastError().text());
    } else {
        QSqlQuery query(m_db);
        query.exec("CREATE TABLE IF NOT EXISTS sensor_log ("
                   "id INTEGER PRIMARY KEY AUTOINCREMENT, "
                   "timestamp DATETIME DEFAULT CURRENT_TIMESTAMP, "
                   "sub_id TEXT, "
                   "event_occurred INTEGER, "
                   "sensor TEXT, "
                   "value REAL)");
    }

    // 2. MQTT TLS 설정 (setSslConfiguration 사용하지 않음)
    QSslConfiguration sslConfig = QSslConfiguration::defaultConfiguration();
    sslConfig.setProtocol(QSsl::TlsV1_3OrLater);

    // Debug: Check all available resource files
    qDebug() << "[MQTT] Available resources:" << QDir(":/").entryList();
    qDebug() << "[MQTT] CA cert directory:" << QDir(":/cert").entryList();
    qDebug() << "[MQTT] CA certs subdirectory:" << QDir(":/cert/ca").entryList();

    QFile caFile(":/cert/ca.crt"); // Try the main cert path first
    qDebug() << "[MQTT] CA 파일 존재 여부 (:/cert/ca.crt):" << QFile::exists(":/cert/ca.crt");

    if (!caFile.exists()) {
        // Try alternative paths
        caFile.setFileName(":/cert/ca/certs/ca.crt");
        qDebug() << "[MQTT] CA 파일 존재 여부 (:/cert/ca/certs/ca.crt):" << QFile::exists(":/cert/ca/certs/ca.crt");
    }

    if (caFile.open(QIODevice::ReadOnly)) {
        QSslCertificate caCert(&caFile);
        QList<QSslCertificate> caList = sslConfig.caCertificates();
        caList.append(caCert);
        sslConfig.setCaCertificates(caList);
        caFile.close();
        qDebug() << "[MQTT] CA 인증서 객체 유효 여부:" << !caCert.isNull();
        logTextEdit->append("CA 인증서 로드 성공");
    } else {
        qWarning() << "[MQTT] CA 인증서 파일을 열 수 없습니다.";
        logTextEdit->append("경고: CA 인증서 파일을 열 수 없습니다. 연결이 실패할 수 있습니다.");
    }

    m_client->setHostname("192.168.0.115");
    m_client->setPort(8883);
    m_client->setClientId("QtWindowsClient");
    m_client->setKeepAlive(60);

    qDebug() << "[MQTT] Connecting to" << m_client->hostname() << "on port" << m_client->port();
    logTextEdit->append(QString("MQTT 서버에 연결 시도: %1:%2").arg(m_client->hostname()).arg(m_client->port()));

    // Ensure connections are made on the main thread
    if (m_client) {
        connect(m_client, &QMqttClient::connected, this, &Mqtt::onConnected, Qt::QueuedConnection);
        connect(m_client, &QMqttClient::disconnected, this, &Mqtt::onDisconnected, Qt::QueuedConnection);
        connect(m_client, &QMqttClient::messageReceived, this, &Mqtt::onMessageReceived, Qt::QueuedConnection);
        connect(m_client, &QMqttClient::errorChanged, this, &Mqtt::onErrorChanged, Qt::QueuedConnection);
    } else {
        qWarning() << "[MQTT] Client is null, cannot establish connections";
        return;
    }

    // 4. TLS 연결 시도 (sslConfig 직접 전달)
    #if (QT_VERSION >= QT_VERSION_CHECK(5, 14, 0))
        m_client->connectToHostEncrypted(sslConfig);
    #else
        m_client->connectToHostEncrypted();
    #endif
    connect(m_client, &QMqttClient::stateChanged,
            this, [](QMqttClient::ClientState state){
                static const char* names[] = { "Disconnected", "Connecting", "Connected" };
                qDebug() << "[MQTT] stateChanged →" << names[state];
            });
    //m_client->connectToHost();

}

Mqtt::~Mqtt()
{
    if (m_db.isOpen()) m_db.close();
}

void Mqtt::setMonitorWindow(MonitorWindow* window) {
    m_monitorWindow = window;
}

void Mqtt::onConnected()
{
    qDebug() << "[MQTT] Successfully connected to broker";
    logTextEdit->append("MQTT 연결 성공!");

    QStringList topics = { "pop/1", "pop/2", "pop/3", "pi/data/fall", "main/result/log/", "main/result/image/", "main/data/Count" };
    for (const QString& topic : topics) {
        auto sub = m_client->subscribe(topic, 1);
        if (!sub)
            logTextEdit->append("subscribe fail: " + topic);
        else
            logTextEdit->append("topic subscribe success: " + topic);
    }
}

void Mqtt::onDisconnected()
{
    qDebug() << "[MQTT] Disconnected from broker";
    logTextEdit->append("MQTT 연결 해제됨");
}

void Mqtt::onErrorChanged(QMqttClient::ClientError error)
{
    QString errorMsg;
    switch (error) {
    case QMqttClient::NoError:
        errorMsg = "No error";
        break;
    case QMqttClient::InvalidProtocolVersion:
        errorMsg = "Invalid protocol version";
        break;
    case QMqttClient::NotAuthorized:
        errorMsg = "Not authorized";
        break;
    case QMqttClient::ServerUnavailable:
        errorMsg = "Server unavailable";
        break;
    case QMqttClient::TransportInvalid:
        errorMsg = "Transport invalid";
        break;
    case QMqttClient::ProtocolViolation:
        errorMsg = "Protocol violation";
        break;
    case QMqttClient::UnknownError:
    default:
        errorMsg = "Unknown error";
        break;
    }
    qDebug() << "[MQTT] Error occurred:" << errorMsg << "Error code:" << error;
    logTextEdit->append("MQTT 에러: " + errorMsg);

    // Add more detailed error information
    if (m_client) {
        qDebug() << "[MQTT] Client state:" << m_client->state();
        logTextEdit->append(QString("클라이언트 상태: %1").arg(m_client->state()));
    }
}

void Mqtt::onMessageReceived(const QByteArray &message, const QMqttTopicName &topic)
{
    QString payloadStr = QString::fromUtf8(message);
    qDebug() << "[MQTT] onMessageReceived\n";

    // // 🔥 Fall Detection 메시지 처리
    // if (topic.name() == "pi/data/fall") {
    //     qDebug() << "[MQTT] ❗ Fall detection message received:" << payloadStr;
    //     logTextEdit->append(QString("❗Fall Detected from: %1").arg(topic.name()));

    //     if (m_monitorWindow) {
    //         QMetaObject::invokeMethod(m_monitorWindow, [=]() {
    //             QMessageBox::critical(m_monitorWindow,
    //                                   "🚨 Fall Detected",
    //                                   "A person has fallen! Immediate action required.",
    //                                   QMessageBox::Ok);
    //             m_monitorWindow->addLogEntry("Fall Detection", "Fall Detected", payloadStr);

    //             // 상태 라벨 변경
    //             if (m_monitorWindow->m_activeAlarmLabel) {
    //                 m_monitorWindow->m_activeAlarmLabel->setText("🚨 Fall Detected!");
    //                 m_monitorWindow->m_activeAlarmLabel->setStyleSheet("color: red; font-size: 18px; font-weight: bold;");
    //             }
    //         }, Qt::QueuedConnection);
    //     }
    // }
    // 🔥 Fall Detection 메시지 처리
    if (topic.name() == "pi/data/fall") {
        qDebug() << "[MQTT] :느낌표: Fall detection message received:" << payloadStr;
        logTextEdit->append(QString(":느낌표:Fall Detected from: %1").arg(topic.name()));
        if (m_monitorWindow) {
            QMetaObject::invokeMethod(m_monitorWindow, [=]() {
                QMessageBox* msgBox = new QMessageBox(m_monitorWindow);
                msgBox->setWindowTitle(":경광등: Fall Detected");
                msgBox->setText("A person has fallen!\nImmediate action required.");
                // msgBox->setIconPixmap(QPixmap(":/new/prefix1/images/emergency.png").scaled(64, 64, Qt::KeepAspectRatio, Qt::SmoothTransformation));
                msgBox->setIcon(QMessageBox::Critical);
                msgBox->setStandardButtons(QMessageBox::Ok);
                msgBox->setStyleSheet(R"(
                    QMessageBox {
                        background-color: #1A1E2A;
                        color: white;
                        font-size: 16px;
                        min-width: 450px;
                        min-height: 250px;
                        border-radius: 12px;
                    }
                    QMessageBox QLabel {
                        color: white;
                        font-size: 20px;
                        font-weight: bold;
                        padding: 30px;
                        qproperty-alignment: AlignCenter;
                    }
                    QMessageBox QPushButton {
                        background-color: #E74C3C;
                        color: white;
                        border: none;
                        border-radius: 8px;
                        padding: 12px 30px;
                        font-size: 16px;
                        font-weight: bold;
                        min-width: 100px;
                        margin: 0 auto;
                    }
                    QMessageBox QPushButton:hover {
                        background-color: #C0392B;
                    }
                    QMessageBox QPushButton:pressed {
                        background-color: #A93226;
                    }
                    QMessageBox .QDialogButtonBox {
                        qproperty-centerButtons: true;
                    }
                )");
                msgBox->exec();
                delete msgBox;
                m_monitorWindow->addLogEntry("Fall Detection", "Fall Detected", payloadStr);
                // 상태 라벨 변경
                if (m_monitorWindow->m_activeAlarmLabel) {
                    m_monitorWindow->m_activeAlarmLabel->setText(":경광등: Fall Detected!");
                    m_monitorWindow->m_activeAlarmLabel->setStyleSheet("color: red; font-size: 18px; font-weight: bold;");
                }
            }, Qt::QueuedConnection);
        }
    }

    if (topic.name() == "main/result/log/") {
        QString eventTime = QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss");
        qDebug() << "[LogEventTime] " << eventTime;

        QString userId = m_monitorWindow->property("m_currentUserId").toString();
        QString jsonStr = QString::fromUtf8(message);
        QSqlQuery q;
        q.prepare("INSERT OR IGNORE INTO fall_events (user_id, event_time, json_data) VALUES (?, ?, ?)");
        q.addBindValue(userId);
        q.addBindValue(eventTime);
        q.addBindValue(jsonStr);
        q.exec();

        qDebug() << "[main/result/log] Stored JSON for" << eventTime << ":" << jsonStr;
    }

    if (topic.name() == "main/result/image/") {
        QString eventTime = QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss");
        qDebug() << "[ImageEventTime] " << eventTime;

        QString imageDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/event_images";
        QDir().mkpath(imageDir);
        QString imagePath = imageDir + QString("/event_%1.jpg").arg(eventTime);

        QFile file(imagePath);
        if (file.open(QIODevice::WriteOnly)) {
            file.write(message);
            file.close();
        }

        // DB에 저장
        QString userId = m_monitorWindow->property("m_currentUserId").toString();
        QSqlQuery q(QSqlDatabase::database());
        q.prepare("UPDATE fall_events SET image_path = ? WHERE user_id =? and event_time = ?");
        q.addBindValue(imagePath);
        q.addBindValue(userId);
        q.addBindValue(eventTime);
        q.exec();

        QString jsonData;
        QSqlQuery q2(QSqlDatabase::database());
        q2.prepare("SELECT json_data FROM fall_events WHERE user_id = ? AND event_time = ?");
        q2.addBindValue(userId);
        q2.addBindValue(eventTime);
        if (q2.exec() && q2.next()) {
            jsonData = q2.value(0).toString();
        }

        // 실시간으로 UI에 반영
        if (m_monitorWindow) {
            QMetaObject::invokeMethod(m_monitorWindow, [=]() {
                m_monitorWindow->addFallEventEntry(eventTime, imagePath, jsonData);
                // QMessageBox::critical(m_monitorWindow,
                //                   "🚨 구조 요청",
                //                   "낙상 이벤트가 감지되었습니다. 구조가 필요합니다.",
                //                   QMessageBox::Ok);
                m_monitorWindow->showCustomFallAlert(eventTime, imagePath, jsonData);

                // 🟥 배너 추가
                if (!m_monitorWindow->m_alertBannerLabel) {
                    m_monitorWindow->m_alertBannerLabel = new QLabel("🚨 낙상 감지! 즉시 확인 바랍니다.");
                    m_monitorWindow->m_alertBannerLabel->setStyleSheet("background-color: red; color: white; font-size: 18px; font-weight: bold; padding: 8px;");
                    m_monitorWindow->m_alertBannerLabel->setAlignment(Qt::AlignCenter);
                    m_monitorWindow->m_alertBannerLabel->setFixedHeight(40);

                    // 기존 레이아웃에 삽입 (HeaderBar 아래)
                    if (auto layout = qobject_cast<QVBoxLayout*>(m_monitorWindow->centralWidget()->layout())) {
                        layout->insertWidget(1, m_monitorWindow->m_alertBannerLabel);  // header 다음에 넣기
                    }
                }

                m_monitorWindow->addLogEntry("Fall Detection", "Rescue Requested", "Image received from fall event");

                if (m_monitorWindow->m_rescueEndButton) {
                    m_monitorWindow->m_rescueEndButton->setEnabled(true);  // 버튼 활성화
                }
            }, Qt::QueuedConnection);
        }

        qDebug() << "[Mqtt] Image saved and DB entry created for" << eventTime;
    }

    // 👇 이하 기존 사람 수 crowd count 처리
    if (topic.name() == "main/data/Count" ||
        topic.name() == "pop/1" ||
        topic.name() == "pop/2" ||
        topic.name() == "pop/3") {

        bool ok;
        int value = payloadStr.toInt(&ok);

        if (ok) {
            qDebug() << "[MQTT] 받은 정수 값:" << value << "토픽:" << topic.name();
            logTextEdit->append(QString("수신된 값 (%1): %2").arg(topic.name()).arg(value));

            if (!m_monitorWindow) return;

            // topic에 따라 cameraId를 지정
            int cameraId = 0;
            if (topic.name() == "main/data/Count") cameraId = 1;
            else if (topic.name() == "pop/1")     cameraId = 2;
            else if (topic.name() == "pop/2")     cameraId = 3;
            else if (topic.name() == "pop/3")     cameraId = 4;

            if (cameraId > 0)
                m_monitorWindow->updateCameraCrowdCount(cameraId, value);  // 내부에서 addLogEntry 포함
        } else {
            qWarning() << "[MQTT] 메시지를 int로 변환 실패:" << payloadStr;
            logTextEdit->append("메시지를 int로 변환 실패: " + payloadStr);
        }
    }
}

bool Mqtt::publishMessage(const QString &topic, const QByteArray &payload, quint8 qos, bool retain)
{
    if (!m_client || m_client->state() != QMqttClient::Connected) {
        qWarning() << "[MQTT] Publish 실패: 클라이언트가 연결되어 있지 않음";
        return false;
    }
    auto subscription = m_client->publish(topic, payload, qos, retain);
    if (!subscription) {
        qWarning() << "[MQTT] Publish 실패: subscription 객체 null";
        return false;
    }
    qDebug() << "[MQTT] Publish 성공:" << topic << payload;
    return true;
}

QMqttClient::ClientState Mqtt::state() const {
    return m_client ? m_client->state() : QMqttClient::Disconnected;
}
