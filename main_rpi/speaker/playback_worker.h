#ifndef PLAYBACK_WORKER_H
#define PLAYBACK_WORKER_H

// Standard Library
#include <vector>
#include <queue>
#include <mutex>
#include <thread>
#include <optional>
#include <condition_variable>
#include <atomic>
#include <chrono>

// ALSA
#include <alsa/asoundlib.h>

/**
 * @brief Represents a chunk of audio data with a timestamp.
 */
struct AudioFrame
{
    std::vector<char> data;                                  ///< Raw PCM audio data.
    std::chrono::steady_clock::time_point timestamp;         ///< Timestamp of when the frame was received.
};

/**
 * @brief Handles threaded audio playback using ALSA.
 */
class PlaybackWorker
{
public:
    /**
     * @brief Constructs the playback worker.
     */
    PlaybackWorker();

    /**
     * @brief Stops playback and releases ALSA resources.
     */
    ~PlaybackWorker();

    /**
     * @brief Initializes the ALSA playback device with the given parameters.
     * @param Sampling rate in Hz.
     * @param Number of channels.
     * @param ALSA format.
     * @return true if successful, false otherwise.
     */
    bool init(int sample_rate, int channels, snd_pcm_format_t format);

    /**
     * @brief Starts the playback thread.
     */
    void start();

    /**
     * @brief Signals playback to stop and waits for thread to finish.
     */
    void stop();

    /**
     * @brief Enqueues an audio frame to be played back.
     * @param An AudioFrame rvalue reference.
     */
    void enqueue(AudioFrame&& frame);

private:
    /**
     * @brief Playback loop executed in a separate thread.
     */
    void playbackLoop();

    /**
     * @brief Releases ALSA and internal queue resources.
     */
    void cleanup();

    // === Members ===
    snd_pcm_t* pcm_handle;
    int sample_rate;
    int channels;
    snd_pcm_format_t format;

    std::queue<AudioFrame> queue;
    std::mutex mutex;
    std::condition_variable cv;
    std::optional<std::thread> thread;

    std::atomic<bool> running;
    std::atomic<bool> initialized;
};

#endif // PLAYBACK_WORKER_H