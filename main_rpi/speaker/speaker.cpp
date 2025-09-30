// Standard Library
#include <cstring>

// Project Headers
#include "speaker.h"

// === Constructor ===
Speaker::Speaker()
    : running(false),
    shutdown_requested(false)
{
}

// === Destructor ===
Speaker::~Speaker()
{
    stop();
}

// === Initializes the internal socket server ===
bool Speaker::init()
{
    return socket.init();
}

// === Main loop for accepting connections and processing packets ===
void Speaker::run()
{
    running = true;

    while (running)
    {
        std::optional<int> fd_opt = socket.pollAccept(1000);
        if (!fd_opt.has_value()) continue;

        int client_fd = fd_opt.value();

        std::vector<char> header;
        std::vector<char> payload;

        while (running)
        {
            RecvStatus status = socket.recvHeaderAndPayload(client_fd, header, payload, shutdown_requested);
            if (status != RecvStatus::SUCCESS) break;

            status = handlePacket(header, payload);
            if (status != RecvStatus::SUCCESS) break;
        }

        socket.closeClient(client_fd);
    }
}

// === Signals stop and shuts down playback and socket ===
void Speaker::stop()
{
    if (!running) return;

    running = false;
    shutdown_requested = true;

    playback_worker.stop();
    socket.shutdown();
}

// === Triggers shutdown externally (e.g., from signal) ===
void Speaker::requestShutdown()
{
    shutdown_requested = true;
    running = false;
}

// === Processes metadata or audio packets from client ===
RecvStatus Speaker::handlePacket(const std::vector<char>& header, const std::vector<char>& payload)
{
    if (header.size() < 3) return RecvStatus::INVALID_MAGIC;

    uint8_t type = header[2];

    if (type == 0x02)  // Metadata packet
    {
        try
        {
            std::string json_str(payload.begin(), payload.end());
            AudioSettings settings = AudioSettings::fromJson(json_str);

            if (!playback_worker.init(settings.sample_rate, settings.channels, settings.toAlsaFormat()))
            {
                return RecvStatus::METADATA_ERROR;
            }

            playback_worker.start();
        }
        catch (...)
        {
            return RecvStatus::METADATA_ERROR;
        }
    }
    else if (type == 0x01)  // Audio frame packet
    {
        AudioFrame frame;
        frame.data = payload;
        frame.timestamp = std::chrono::steady_clock::now();

        playback_worker.enqueue(std::move(frame));
    }
    else
    {
        return RecvStatus::UNKNOWN_PACKET;
    }

    return RecvStatus::SUCCESS;
}