#ifndef MQTT_H
#define MQTT_H

#include <QWidget>
#include <QtMqtt/QMqttClient>
#include <QSqlDatabase>
#include "monitorwindow.h"

class QTextEdit;
class QJsonObject;

class Mqtt : public QWidget
{
    Q_OBJECT

public:
    QMqttClient::ClientState state() const;
    explicit Mqtt(QWidget *parent = nullptr);
    ~Mqtt();
    void setMonitorWindow(MonitorWindow* window);
    bool publishMessage(const QString &topic, const QByteArray &payload, quint8 qos = 1, bool retain = false);

Q_SIGNALS:
    void connected();

private Q_SLOTS:
    void onConnected();
    void onDisconnected();
    void onMessageReceived(const QByteArray &message, const QMqttTopicName &topic);
    void onErrorChanged(QMqttClient::ClientError error);

private:
    QMqttClient *m_client;
    QSqlDatabase m_db;
    QTextEdit *logTextEdit;
    MonitorWindow* m_monitorWindow = nullptr;
};

#endif // MQTT_H
