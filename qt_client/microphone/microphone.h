#ifndef MICROPHONE_H
#define MICROPHONE_H

// Qt Library
#include <QObject>
#include <QString>

// Project headers
#include "microphone_input.h"
#include "microphone_socket.h"

/**
 * @brief Orchestrates audio capture and network streaming.
 *        Acts as a bridge between MicrophoneInput and MicrophoneSocket.
 */
class Microphone : public QObject
{
    Q_OBJECT

public:
    /**
     * @brief Constructs the Microphone object.
     * @param The QObject parent.
     */
    explicit Microphone(QObject* parent = nullptr);

    /**
     * @brief Cleans up any resources.
     */
    ~Microphone() override;

    /**
     * @brief Starts audio capture and streaming.
     * @param The server IP to connect to.
     * @param The server port.
     * @return true if successfully started.
     */
    bool start(const QString& ip, quint16 port);

    /**
     * @brief Stops all audio and network activity.
     */
    void stop();

    /**
     * @brief Returns true if streaming is active.
     */
    bool isRunning() const { return is_running; }

private:
    MicrophoneInput* input = nullptr;
    MicrophoneSocket* socket = nullptr;
    bool is_running = false;
};

#endif // MICROPHONE_H
