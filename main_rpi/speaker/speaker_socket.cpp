// System Library
#include <unistd.h>
#include <fcntl.h>
#include <cstring>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <algorithm>

// Project headers
#include "speaker_socket.h"

// === Constructor ===
SpeakerSocket::SpeakerSocket(int port)
    : server_fd(-1),
    port(port)
{
}

// === Destructor ===
SpeakerSocket::~SpeakerSocket()
{
    shutdown();
}

// === Initializes the server socket and sets it to non-blocking ===
bool SpeakerSocket::init()
{
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = INADDR_ANY;

    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0)
        return false;

    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    if (bind(server_fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) return false;

    if (listen(server_fd, SOMAXCONN) < 0) return false;

    fcntl(server_fd, F_SETFL, O_NONBLOCK);

    return true;
}

// === Closes all client connections and shuts down the server socket ===
void SpeakerSocket::shutdown()
{
    for (auto& pfd : client_fds)
    {
        if (pfd.fd >= 0)
        {
            close(pfd.fd);
        }
    }
    client_fds.clear();

    if (server_fd >= 0)
    {
        close(server_fd);
        server_fd = -1;
    }
}

// === Polls for new client or active client data ===
std::optional<int> SpeakerSocket::pollAccept(int timeout_ms)
{
    std::vector<pollfd> fds;
    fds.reserve(1 + client_fds.size());

    fds.push_back({ server_fd, POLLIN, 0 });

    for (auto& client : client_fds)
    {
        fds.push_back({ client.fd, POLLIN, 0 });
    }

    int ret = poll(fds.data(), fds.size(), timeout_ms);
    if (ret <= 0) return std::nullopt;

    if (fds[0].revents & POLLIN)
    {
        sockaddr_in client_addr{};
        socklen_t len = sizeof(client_addr);
        int client_fd = accept(server_fd, reinterpret_cast<sockaddr*>(&client_addr), &len);
        if (client_fd >= 0)
        {
            fcntl(client_fd, F_SETFL, O_NONBLOCK);
            client_fds.push_back({ client_fd, POLLIN, 0 });
            return client_fd;
        }
    }

    for (size_t i = 1; i < fds.size(); ++i)
    {
        if (fds[i].revents & POLLIN)
        {
            return fds[i].fd;
        }
    }

    return std::nullopt;
}

// === Receives a packet consisting of a header and payload from a client socket ===
RecvStatus SpeakerSocket::recvHeaderAndPayload(int fd, std::vector<char>& header, std::vector<char>& payload, bool shutdown_requested)
{
    constexpr int header_size = 8;
    header.resize(header_size);

    if (!recvExactWithTimeout(fd, header.data(), header.size(), 3000))
    {
        return shutdown_requested ? RecvStatus::SHUTDOWN_REQUESTED : RecvStatus::CLIENT_DISCONNECTED;
    }

    uint16_t magic = *reinterpret_cast<uint16_t*>(&header[0]);
    if (magic != 0xAA55) return RecvStatus::INVALID_MAGIC;

    uint32_t length = *reinterpret_cast<uint32_t*>(&header[4]);
    payload.resize(length);

    if (!recvExactWithTimeout(fd, payload.data(), length, 3000))
    {
        return RecvStatus::PAYLOAD_ERROR;
    }

    return RecvStatus::SUCCESS;
}

// === Receives a fixed-length buffer from a client socket with timeout ===
bool SpeakerSocket::recvExactWithTimeout(int fd, void* buf, size_t len, int timeout_ms)
{
    size_t total = 0;
    char* buffer = static_cast<char*>(buf);

    while (total < len)
    {
        pollfd pfd{ fd, POLLIN, 0 };
        int ret = poll(&pfd, 1, timeout_ms);

        if (ret <= 0) return false;
        if (!(pfd.revents & POLLIN)) return false;

        ssize_t received = recv(fd, buffer + total, len - total, 0);
        if (received <= 0)
        {
            return false;
        }

        total += received;
    }

    return true;
}

// === Closes the specified client socket and removes it from tracking ===
void SpeakerSocket::closeClient(int fd)
{
    close(fd);
    client_fds.erase(std::remove_if(client_fds.begin(), client_fds.end(), [fd](const pollfd& pfd) { return pfd.fd == fd; }), client_fds.end());
}