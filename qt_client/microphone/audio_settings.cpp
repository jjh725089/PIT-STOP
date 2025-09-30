// Qt Library
#include <QString>

// Project headers
#include "audio_settings.h"

// === Returns a default audio format: 48kHz stereo float ===
QAudioFormat AudioSettings::defaultFormat()
{
    QAudioFormat format;
    format.setSampleRate(48000);
    format.setChannelCount(2);
    format.setSampleFormat(QAudioFormat::Float);
    return format;
}

// === Converts audio format into a JSON object (Always outputs mono, int16, little endian settings) ===
QJsonObject AudioSettings::toJson(const QAudioFormat& format)
{
    QJsonObject obj;
    obj["sample_rate"] = format.sampleRate();
    obj["channels"] = 1;  // Downmix to mono on client side
    obj["format"] = "int16";
    obj["endianness"] = "little";
    return obj;
}

// === Reconstructs an audio format from JSON description ===
QAudioFormat AudioSettings::fromJson(const QJsonObject& json)
{
    QAudioFormat format;
    format.setSampleRate(json["sample_rate"].toInt(48000));
    format.setChannelCount(json["channels"].toInt(1));

    const QString fmt = json["format"].toString();
    if (fmt == "int16") {
        format.setSampleFormat(QAudioFormat::Int16);
    } else if (fmt == "float") {
        format.setSampleFormat(QAudioFormat::Float);
    }

    return format;
}
