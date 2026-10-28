// SPDX-License-Identifier: MIT
// NexKey - talking to the virtual keyboard helper.

#include "nexkey/UinputClient.hpp"

#include <cerrno>
#include <cstring>
#include <string>
#include <string_view>
#include <utility>

#include <poll.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

namespace nexkey::uinput {

namespace {

/// The socket is created by a helper that may not be there, and every call in
/// this file has to survive that without raising SIGPIPE or blocking.
int open_connection(const std::string& path, std::chrono::milliseconds timeout)
{
    if (path.empty() || path.size() >= sizeof(sockaddr_un{}.sun_path)) {
        return -1;
    }

    const int fd = ::socket(AF_UNIX, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0);
    if (fd < 0) {
        return -1;
    }

    struct sockaddr_un address {};
    address.sun_family = AF_UNIX;
    std::memcpy(address.sun_path, path.c_str(), path.size() + 1);

    int result = ::connect(fd, reinterpret_cast<struct sockaddr*>(&address), sizeof(address));
    if (result != 0 && errno == EINPROGRESS) {
        struct pollfd wait {};
        wait.fd = fd;
        wait.events = POLLOUT;
        const int ready = ::poll(&wait, 1, static_cast<int>(timeout.count()));
        if (ready <= 0) {
            ::close(fd);
            return -1;
        }
        // A failed connect reports the reason through SO_ERROR, not through the
        // poll result.
        int error = 0;
        socklen_t length = sizeof(error);
        if (::getsockopt(fd, SOL_SOCKET, SO_ERROR, &error, &length) != 0 || error != 0) {
            ::close(fd);
            return -1;
        }
        result = 0;
    }
    if (result != 0) {
        ::close(fd);
        return -1;
    }
    return fd;
}

bool write_all(int fd, std::string_view text, std::chrono::milliseconds timeout)
{
    std::size_t written = 0;
    while (written < text.size()) {
        const ssize_t result = ::send(fd, text.data() + written, text.size() - written, MSG_NOSIGNAL);
        if (result > 0) {
            written += static_cast<std::size_t>(result);
            continue;
        }
        if (result < 0 && (errno == EINTR)) {
            continue;
        }
        if (result < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
            struct pollfd wait {};
            wait.fd = fd;
            wait.events = POLLOUT;
            if (::poll(&wait, 1, static_cast<int>(timeout.count())) <= 0) {
                return false;
            }
            continue;
        }
        return false;
    }
    return true;
}

/// Read one line, or give up. Kept byte by byte on purpose: a reply is at most
/// a dozen bytes and this way a partial read cannot lose the rest of it.
bool read_line(int fd, std::string& line, std::chrono::milliseconds timeout)
{
    line.clear();
    while (line.size() < 128) {
        struct pollfd wait {};
        wait.fd = fd;
        wait.events = POLLIN;
        const int ready = ::poll(&wait, 1, static_cast<int>(timeout.count()));
        if (ready <= 0) {
            return false;
        }
        char character = 0;
        const ssize_t result = ::recv(fd, &character, 1, 0);
        if (result == 0) {
            return false;
        }
        if (result < 0) {
            if (errno == EINTR) {
                continue;
            }
            return false;
        }
        if (character == '\n') {
            return true;
        }
        line.push_back(character);
    }
    return false;
}

} // namespace

Client::Client(std::string socket_path)
    : m_path(std::move(socket_path))
{
}

Client::~Client()
{
    disconnect();
}

Client::Client(Client&& other) noexcept
    : m_path(std::move(other.m_path))
    , m_fd(other.m_fd)
{
    other.m_fd = -1;
}

Client& Client::operator=(Client&& other) noexcept
{
    if (this != &other) {
        disconnect();
        m_path = std::move(other.m_path);
        m_fd = other.m_fd;
        other.m_fd = -1;
    }
    return *this;
}

void Client::set_socket_path(std::string path)
{
    if (path != m_path) {
        disconnect();
        m_path = std::move(path);
    }
}

void Client::disconnect()
{
    if (m_fd >= 0) {
        ::close(m_fd);
        m_fd = -1;
    }
}

bool Client::connect_socket()
{
    if (m_fd >= 0) {
        return true;
    }
    m_fd = open_connection(m_path, kConnectTimeout);
    return m_fd >= 0;
}

Submit Client::exchange(const std::string& line, std::uint32_t& performed)
{
    if (!connect_socket()) {
        return Submit::NoHelper;
    }
    if (!write_all(m_fd, line, kReplyTimeout)) {
        disconnect();
        return Submit::Failed;
    }

    std::string reply;
    if (!read_line(m_fd, reply, kReplyTimeout)) {
        disconnect();
        return Submit::Failed;
    }
    if (parse_ok(reply, performed)) {
        return Submit::Sent;
    }
    // An error reply means the helper is alive but unhappy. Keeping the
    // connection is what makes the next request cheap.
    return Submit::Refused;
}

Submit Client::submit(const Request& request, std::uint32_t& performed)
{
    performed = 0;
    if (!request.delays.valid() || request.backspaces > kMaxBackspaces) {
        return Submit::Refused;
    }
    return exchange(encode_request(request), performed);
}

bool Client::ping()
{
    std::uint32_t performed = 0;
    static_cast<void>(performed);
    if (!connect_socket()) {
        return false;
    }
    if (!write_all(m_fd, std::string(kPing) + "\n", kReplyTimeout)) {
        disconnect();
        return false;
    }
    std::string reply;
    if (!read_line(m_fd, reply, kReplyTimeout)) {
        disconnect();
        return false;
    }
    return reply == kPong;
}

} // namespace nexkey::uinput
