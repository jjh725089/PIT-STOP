// Qt Library
#include <QDataStream>
#include <QHostAddress>
#include <QJsonDocument>
#include <QDebug>

// Project headers
#include "microphone_socket.h"

// === Constructor ===
MicrophoneSocket::MicrophoneSocket(QObject* parent)
    : QObject(parent)
{
}

// === Destructor ===
MicrophoneSocket::~MicrophoneSocket()
{
    disconnectFromServer();
}

// === Attempts to connect to a server ===
bool MicrophoneSocket::connectToServer(const QString& ip, quint16 port)
{
    if (socket) return false;

    socket = new QTcpSocket(this);
    socket->setSocketOption(QAbstractSocket::LowDelayOption, 1);

    connect(socket, &QTcpSocket::errorOccurred, this, [this](QAbstractSocket::SocketError error) { Q_UNUSED(error); qWarning() << "[Socket] Error:" << socket->errorString(); });

    socket->connectToHost(ip, port);

    if (!socket->waitForConnected(5000))
    {
        qWarning() << "[Socket] Failed to connect to" << ip << ":" << port;
        disconnectFromServer();
        return false;
    }

    last_sent.start();
    return true;
}

// === Closes and cleans up the connection ===
void MicrophoneSocket::disconnectFromServer()
{
    stopKeepAlive();

    if (socket)
    {
        if (socket->state() == QAbstractSocket::ConnectedState)
        {
            socket->disconnectFromHost();
            if (socket->state() != QAbstractSocket::UnconnectedState)
                socket->waitForDisconnected(1000);
        }

        socket->deleteLater();
        socket = nullptr;
    }
}

// === Sends audio format metadata to the server ===
void MicrophoneSocket::sendMetadata(const QJsonObject& metadata)
{
    if (!socket || socket->state() != QAbstractSocket::ConnectedState) return;

    const QByteArray payload = QJsonDocument(metadata).toJson(QJsonDocument::Compact);

    QByteArray packet;
    QDataStream stream(&packet, QIODevice::WriteOnly);
    stream.setByteOrder(QDataStream::LittleEndian);

    stream << quint16(0xAA55)   // magic
           << quint8(0x02)      // metadata
           << quint8(0x00)      // reserved
           << quint32(payload.size());

    packet.append(payload);
    socket->write(packet);
    socket->flush();
}

// === Sends a block of PCM audio data to the server ===
void MicrophoneSocket::sendAudio(const QByteArray& pcm_data)
{
    if (!socket || socket->state() != QAbstractSocket::ConnectedState) return;

    QByteArray packet;
    QDataStream stream(&packet, QIODevice::WriteOnly);
    stream.setByteOrder(QDataStream::LittleEndian);

    stream << quint16(0xAA55)  // magic
           << quint8(0x01)     // audio
           << quint8(0x00)     // reserved
           << quint32(pcm_data.size());

    packet.append(pcm_data);
    socket->write(packet);

    last_sent.restart();
}

// === Starts sending periodic silent packets to keep connection alive ===
void MicrophoneSocket::startKeepAlive()
{
    if (keep_alive_timer) return;

    keep_alive_timer = new QTimer(this);
    connect(keep_alive_timer, &QTimer::timeout, this, &MicrophoneSocket::sendSilentPacket);
    keep_alive_timer->start(500);
}

// === Stops the keep-alive mechanism ===
void MicrophoneSocket::stopKeepAlive()
{
    if (keep_alive_timer)
    {
        keep_alive_timer->stop();
        keep_alive_timer->deleteLater();
        keep_alive_timer = nullptr;
    }
}

// === Sends a silent packet if no audio was sent recently ===
void MicrophoneSocket::sendSilentPacket()
{
    if (!socket || socket->state() != QAbstractSocket::ConnectedState) return;

    if (last_sent.elapsed() < 1000) return;

    const QByteArray silence(480 * sizeof(int16_t), 0);  // 10ms of silence @48kHz mono
    sendAudio(silence);
}
