// Standard Library
#include <string>

// JSON
#include <nlohmann/json.hpp>

// Project headers
#include "audio_settings.h"

// === Parses a JSON string and creates an AudioSettings object ===
AudioSettings AudioSettings::fromJson(const std::string& json_str)
{
    AudioSettings settings;

    try
    {
        auto json = nlohmann::json::parse(json_str);

        settings.sample_rate = json.value("sample_rate", 48000);
        settings.channels = json.value("channels", 1);
        settings.format = json.value("format", "int16");
    }
    catch (...)
    {
        // fallback to default-initialized values
    }

    return settings;
}

// === Converts the current AudioSettings to a JSON-formatted string ===
std::string AudioSettings::toJson() const
{
    nlohmann::json json;
    json["sample_rate"] = sample_rate;
    json["channels"] = channels;
    json["format"] = format;

    return json.dump();
}

// === Converts the format string to a corresponding ALSA format enum ===
snd_pcm_format_t AudioSettings::toAlsaFormat() const
{
    if (format == "int16") return SND_PCM_FORMAT_S16_LE;
    else if (format == "float32" || format == "float") return SND_PCM_FORMAT_FLOAT_LE;
    else return SND_PCM_FORMAT_UNKNOWN;
}