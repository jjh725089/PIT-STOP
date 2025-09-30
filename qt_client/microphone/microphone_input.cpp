// Qt Library
#include <QMediaDevices>
#include <QAudioDevice>
#include <QDebug>

// Standard Library
#include <cmath>
#include <algorithm>

// Project headers
#include "microphone_input.h"

// === Constructor ===
MicrophoneInput::MicrophoneInput(QObject* parent)
    : QObject(parent)
{
}

// === Destructor ===
MicrophoneInput::~MicrophoneInput()
{
    stop();
}

// === Starts capturing audio using the specified format ===
bool MicrophoneInput::start(const QAudioFormat& format)
{
    if (audio_source) return false;

    audio_format = format;

    const QAudioDevice input_device = QMediaDevices::defaultAudioInput();
    audio_source = new QAudioSource(input_device, format, this);
    audio_io_device = audio_source->start();

    if (!audio_io_device)
    {
        delete audio_source;
        audio_source = nullptr;
        return false;
    }

    connect(audio_io_device, &QIODevice::readyRead, this, &MicrophoneInput::handleReadyRead);
    return true;
}

// === Stops audio capture and releases internal resources ===
void MicrophoneInput::stop()
{
    if (audio_source)
    {
        audio_source->stop();
        audio_source->deleteLater();
        audio_source = nullptr;
    }

    audio_io_device = nullptr;
}

// === Slot called when audio data is available for reading ===
void MicrophoneInput::handleReadyRead()
{
    if (!audio_io_device) return;

    const QByteArray raw = audio_io_device->readAll();

    const float* samples = reinterpret_cast<const float*>(raw.constData());
    const int stereo_frames = raw.size() / (sizeof(float) * 2);

    QByteArray mono_data;
    mono_data.resize(stereo_frames * sizeof(int16_t));
    int16_t* output = reinterpret_cast<int16_t*>(mono_data.data());

    for (int i = 0; i < stereo_frames; ++i)
    {
        const float left = samples[2 * i];
        const float right = samples[2 * i + 1];
        float mono = 0.5f * (left + right);
        mono = std::clamp(mono, -1.0f, 1.0f);
        output[i] = static_cast<int16_t>(std::round(mono * 32767.0f));
    }

    emit audioCaptured(mono_data);
}
