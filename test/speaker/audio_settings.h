#ifndef AUDIO_SETTINGS_H
#define AUDIO_SETTINGS_H

// Standard Library
#include <string>

// ALSA
#include <alsa/asoundlib.h>

/**
 * @brief Represents audio configuration parameters such as sample rate, channels, and format.
 */
class AudioSettings
{
public:
    /**
     * @brief Default constructor with standard values.
     */
    AudioSettings() = default;

    /**
     * @brief Creates an AudioSettings object from a JSON string.
     * @param JSON-formatted string representing audio settings.
     * @return The parsed audio settings.
     */
    static AudioSettings fromJson(const std::string& json_str);

    /**
     * @brief Serializes the audio settings to a JSON string.
     * @return JSON-formatted string.
     */
    std::string toJson() const;

    /**
     * @brief Converts the internal format string to ALSA format enum.
     * @return ALSA-compatible format enum.
     */
    snd_pcm_format_t toAlsaFormat() const;

    // === Members ===
    int sample_rate = 48000;
    int channels = 1;
    std::string format = "int16";
};

#endif // AUDIO_SETTINGS_H