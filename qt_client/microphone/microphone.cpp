// Qt Library
#include <QDebug>

// Project headers
#include "microphone.h"
#include "audio_settings.h"

// === Constructor ===
Microphone::Microphone(QObject* parent)
    : QObject(parent)
{
}

// === Destructor ===
Microphone::~Microphone()
{
    stop();
}

// === Starts audio capture and streaming ===
bool Microphone::start(const QString& ip, quint16 port)
{
    if (is_running) return false;

    const QAudioFormat format = AudioSettings::defaultFormat();

    socket = new MicrophoneSocket(this);
    if (!socket->connectToServer(ip, port))
    {
        delete socket;
        socket = nullptr;
        return false;
    }

    input = new MicrophoneInput(this);
    if (!input->start(format))
    {
        socket->disconnectFromServer();
        delete socket;
        socket = nullptr;

        delete input;
        input = nullptr;
        return false;
    }

    connect(input, &MicrophoneInput::audioCaptured,
            socket, &MicrophoneSocket::sendAudio);

    socket->sendMetadata(AudioSettings::toJson(format));
    socket->startKeepAlive();

    is_running = true;
    return true;
}

// === Stops all audio and network activity ===
void Microphone::stop()
{
    if (!is_running) return;

    if (input)
    {
        input->stop();
        delete input;
        input = nullptr;
    }

    if (socket)
    {
        socket->disconnectFromServer();
        delete socket;
        socket = nullptr;
    }

    is_running = false;
}
