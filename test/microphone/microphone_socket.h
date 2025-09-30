#ifndef MICROPHONE_SOCKET_H
#define MICROPHONE_SOCKET_H

// Qt Library
#include <QObject>
#include <QTcpSocket>
#include <QTimer>
#include <QElapsedTimer>
#include <QJsonObject>

/**
 * @brief Handles TCP socket connection and audio/metadata transmission to a server.
 */
class MicrophoneSocket : public QObject
{
    Q_OBJECT

public:
    /**
     * @brief Constructs the MicrophoneSocket object.
     * @param The QObject parent.
     */
    explicit MicrophoneSocket(QObject* parent = nullptr);

    /**
     * @brief Disconnects and cleans up.
     */
    ~MicrophoneSocket() override;

    /**
     * @brief Attempts to connect to a server.
     * @param Target IP address.
     * @param Target port number.
     * @return true if connection succeeds.
     */
    bool connectToServer(const QString& ip, quint16 port);

    /**
     * @brief Closes and cleans up the connection.
     */
    void disconnectFromServer();

    /**
     * @brief Sends audio format metadata to the server.
     */
    void sendMetadata(const QJsonObject& metadata);

    /**
     * @brief Sends a block of PCM audio data to the server.
     */
    void sendAudio(const QByteArray& pcm_data);

    /**
     * @brief Starts sending periodic silent packets to keep connection alive.
     */
    void startKeepAlive();

    /**
     * @brief Stops the keep-alive mechanism.
     */
    void stopKeepAlive();

private slots:
    /**
     * @brief Sends a silent packet if no audio was sent recently.
     */
    void sendSilentPacket();

private:
    QTcpSocket* socket = nullptr;
    QTimer* keep_alive_timer = nullptr;
    QElapsedTimer last_sent;
};

#endif // MICROPHONE_SOCKET_H
