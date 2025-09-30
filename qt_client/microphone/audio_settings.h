#ifndef AUDIO_SETTINGS_H
#define AUDIO_SETTINGS_H

// Qt Library
#include <QAudioFormat>
#include <QJsonObject>

/**
 * @brief Utility class for handling default audio format and JSON conversion.
 */
class AudioSettings
{
public:
    /**
     * @brief Returns the default audio format (48kHz stereo float).
     */
    static QAudioFormat defaultFormat();

    /**
     * @brief Converts a QAudioFormat to a JSON representation.
     * @param The audio format to convert.
     * @return QJsonObject with metadata.
     */
    static QJsonObject toJson(const QAudioFormat& format);

    /**
     * @brief Constructs a QAudioFormat from a JSON object.
     * @param The input JSON.
     * @return Parsed QAudioFormat.
     */
    static QAudioFormat fromJson(const QJsonObject& json);
};

#endif // AUDIO_SETTINGS_H
