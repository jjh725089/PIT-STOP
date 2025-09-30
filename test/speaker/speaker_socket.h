#ifndef SPEAKER_SOCKET_H
#define SPEAKER_SOCKET_H

// Standard Library
#include <vector>
#include <optional>
#include <chrono>

// System Library
#include <netinet/in.h>
#include <poll.h>

/**
 * @brief Status of received packet over socket.
 */
enum class RecvStatus
{
    SUCCESS,                ///< Successfully received data.
    CLIENT_DISCONNECTED,    ///< Client disconnected.
    SHUTDOWN_REQUESTED,     ///< Shutdown was requested.
    SOCKET_ERROR,           ///< General socket error.
    INVALID_MAGIC,          ///< Packet header magic is invalid.
    PAYLOAD_ERROR,          ///< Payload could not be read.
    METADATA_ERROR,         ///< Metadata parsing failed.
    UNKNOWN_PACKET          ///< Unrecognized packet type.
};

/**
 * @brief TCP server socket class for accepting and reading audio packets.
 */
class SpeakerSocket
{
public:
    /**
     * @brief Constructor with optional port number.
     * @param Port number to bind to (default: 8888).
     */
    explicit SpeakerSocket(int port = 8888);

    /**
     * @brief Destructor. Automatically shuts down open sockets.
     */
    ~SpeakerSocket();

    /**
     * @brief Initializes the server socket and sets it to non-blocking.
     * @return true if successful, false otherwise.
     */
    bool init();

    /**
     * @brief Closes all client connections and shuts down the server socket.
     */
    void shutdown();

    /**
     * @brief Polls for new client or active client data.
     * @param Timeout in milliseconds.
     * @return File descriptor of the ready socket, or std::nullopt on timeout or error.
     */
    std::optional<int> pollAccept(int timeout_ms = 1000);

    /**
     * @brief Receives a packet consisting of a header and payload from a client socket.
     * @param Client socket file descriptor.
     * @param Buffer to store the received header.
     * @param Buffer to store the received payload.
     * @param Whether the system is shutting down.
     * @return Describing the outcome.
     */
    RecvStatus recvHeaderAndPayload(int fd, std::vector<char>& header, std::vector<char>& payload, bool shutdown_requested = false);

    /**
     * @brief Receives a fixed-length buffer from a client socket with timeout.
     * @param Client socket file descriptor.
     * @param Buffer pointer to receive data into.
     * @param Expected number of bytes.
     * @param Timeout in milliseconds.
     * @return true if all bytes were received, false otherwise.
     */
    bool recvExactWithTimeout(int fd, void* buf, size_t len, int timeout_ms = 3000);

    /**
     * @brief Closes the specified client socket and removes it from tracking.
     * @param File descriptor to close.
     */
    void closeClient(int fd);

private:
    // === Members ===
    int server_fd;
    int port;
    std::vector<pollfd> client_fds;
};

#endif // SPEAKER_SOCKET_H