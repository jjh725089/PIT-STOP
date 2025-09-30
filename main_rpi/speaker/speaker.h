#ifndef SPEAKER_H
#define SPEAKER_H

// Project Headers
#include "audio_settings.h"
#include "playback_worker.h"
#include "speaker_socket.h"

/**
 * @brief Orchestrates audio playback by receiving data from a socket and forwarding it to the audio worker.
 */
class Speaker
{
public:
    /**
     * @brief Constructs the speaker object.
     */
    Speaker();

    /**
     * @brief Cleans up resources on destruction.
     */
    ~Speaker();

    /**
     * @brief Initializes the internal socket server.
     * @return true if successful, false otherwise.
     */
    bool init();

    /**
     * @brief Runs the main loop to accept clients and process audio/metadata packets.
     */
    void run();

    /**
     * @brief Gracefully stops the speaker and shuts down all subsystems.
     */
    void stop();

    /**
     * @brief Triggers a shutdown request (e.g., from signal handler).
     */
    void requestShutdown();

private:
    /**
     * @brief Handles a complete packet received from a client.
     * @param The header bytes.
     * @param The payload bytes.
     * @return Indicating the result of processing.
     */
    RecvStatus handlePacket(const std::vector<char>& header, const std::vector<char>& payload);

    // === Members ===
    SpeakerSocket socket;
    PlaybackWorker playback_worker;

    std::atomic<bool> running;
    std::atomic<bool> shutdown_requested;
};

#endif // SPEAKER_H