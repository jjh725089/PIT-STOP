// Standard Library
#include <iostream>
#include <utility>

// Project headers
#include "playback_worker.h"

namespace {
    constexpr int k_max_playback_delay_ms = 5;
}

// === Constructor ===
PlaybackWorker::PlaybackWorker()
    : pcm_handle(nullptr),
    sample_rate(48000),
    channels(1),
    format(SND_PCM_FORMAT_S16_LE),
    running(false),
    initialized(false)
{
}

// === Destructor ===
PlaybackWorker::~PlaybackWorker()
{
    stop();
}

// === Initializes the ALSA playback device with the given parameters ===
bool PlaybackWorker::init(int sample_rate, int channels, snd_pcm_format_t format)
{
    cleanup();  // Ensure previous resources are cleared

    this->sample_rate = sample_rate;
    this->channels = channels;
    this->format = format;

    snd_pcm_hw_params_t* hw_params;
    snd_pcm_hw_params_alloca(&hw_params);

    if (snd_pcm_open(&pcm_handle, "default", SND_PCM_STREAM_PLAYBACK, 0) < 0)
    {
        pcm_handle = nullptr;
        return false;
    }

    try
    {
        if (snd_pcm_hw_params_any(pcm_handle, hw_params) < 0 ||
            snd_pcm_hw_params_set_access(pcm_handle, hw_params, SND_PCM_ACCESS_RW_INTERLEAVED) < 0 ||
            snd_pcm_hw_params_set_format(pcm_handle, hw_params, format) < 0 ||
            snd_pcm_hw_params_set_channels(pcm_handle, hw_params, channels) < 0 ||
            snd_pcm_hw_params_set_rate(pcm_handle, hw_params, sample_rate, 0) < 0 ||
            snd_pcm_hw_params_set_buffer_size(pcm_handle, hw_params, 2048) < 0 ||
            snd_pcm_hw_params_set_period_size(pcm_handle, hw_params, 512, 0) < 0 ||
            snd_pcm_hw_params(pcm_handle, hw_params) < 0)
        {
            cleanup();
            return false;
        }

        initialized = true;
        return true;
    }
    catch (...)
    {
        cleanup();  // Prevent resource leak on exception
        return false;
    }
}

// === Starts the playback thread ===
void PlaybackWorker::start()
{
    if (!initialized || running) return;

    running = true;
    thread.emplace(&PlaybackWorker::playbackLoop, this);
}

// === Signals playback to stop and waits for thread to finish ===
void PlaybackWorker::stop()
{
    if (!running)
        return;

    running = false;
    cv.notify_all();

    if (thread && thread->joinable())
    {
        try
        {
            thread->join();
        }
        catch (...)
        {
            // Silently ignore join failure
        }
    }

    thread.reset();
    cleanup();
}

// === Enqueues an audio frame to be played back ===
void PlaybackWorker::enqueue(AudioFrame&& frame)
{
    {
        std::lock_guard<std::mutex> lock(mutex);
        queue.push(std::move(frame));
    }
    cv.notify_one();
}

// === Playback loop executed in a separate thread ===
void PlaybackWorker::playbackLoop()
{
    try
    {
        while (running || (!queue.empty() && pcm_handle))
        {
            AudioFrame frame;

            {
                std::unique_lock<std::mutex> lock(mutex);
                cv.wait(lock, [this] {
                    return !queue.empty() || !running || pcm_handle == nullptr;
                    });

                if (!running && queue.empty())
                    break;

                if (!pcm_handle)
                    break;

                frame = std::move(queue.front());
                queue.pop();
            }

            auto now = std::chrono::steady_clock::now();
            auto delay_ms = std::chrono::duration_cast<std::chrono::milliseconds>(now - frame.timestamp).count();

            if (delay_ms > k_max_playback_delay_ms)
            {
                continue;  // Discard stale frame
            }

            size_t bytes_per_frame = snd_pcm_format_physical_width(format) / 8;
            snd_pcm_sframes_t frame_count = frame.data.size() / bytes_per_frame;

            snd_pcm_sframes_t written = snd_pcm_writei(pcm_handle, frame.data.data(), frame_count);
            if (written < 0)
            {
                snd_pcm_prepare(pcm_handle);
            }
        }
    }
    catch (...)
    {
        // Prevent thread crash
    }
}

// === Releases ALSA and internal queue resources ===
void PlaybackWorker::cleanup()
{
    if (pcm_handle)
    {
        snd_pcm_close(pcm_handle);
        pcm_handle = nullptr;
    }

    initialized = false;

    {
        std::lock_guard<std::mutex> lock(mutex);
        std::queue<AudioFrame> empty;
        std::swap(queue, empty);
    }
}