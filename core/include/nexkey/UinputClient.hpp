// SPDX-License-Identifier: MIT
// NexKey - the frontend side of the virtual keyboard helper.
//
// The client is deliberately thin and never blocks the input method: the
// helper answers within milliseconds, but a hung helper must not hang typing.
// A failed submission simply moves the word to the next rung of the delivery
// ladder, which the caller can see from the return value.

#pragma once

#include "nexkey/Uinput.hpp"

#include <chrono>
#include <cstdint>
#include <string>

namespace nexkey::uinput {

/// What happened to one submission.
enum class Submit : std::uint8_t {
    Sent,    ///< The helper took the request.
    NoHelper,///< There is no usable socket: the helper is not running.
    Refused, ///< The helper answered with an error.
    Failed,  ///< Broken connection or a write that went nowhere.
};

class Client {
public:
    /// How long a connect is allowed to take. It is a unix socket in the
    /// runtime directory, so anything slower than this means the helper is
    /// wedged and the word should take another path.
    static constexpr auto kConnectTimeout = std::chrono::milliseconds{40};

    /// How long to wait for the reply line before calling the submission done.
    static constexpr auto kReplyTimeout = std::chrono::milliseconds{25};

    Client() = default;
    explicit Client(std::string socket_path);
    ~Client();

    Client(const Client&) = delete;
    Client& operator=(const Client&) = delete;
    Client(Client&& other) noexcept;
    Client& operator=(Client&& other) noexcept;

    /// Use a different socket, for tests and for the --socket option.
    void set_socket_path(std::string path);
    [[nodiscard]] const std::string& socket_path() const noexcept { return m_path; }

    /// Ask for a burst. `performed` receives the count the helper confirmed,
    /// which is the number of backspaces actually pressed.
    Submit submit(const Request& request, std::uint32_t& performed);

    /// True when a helper answers a ping. Cheap enough to call when a word
    /// starts and the answer is cached by the caller.
    [[nodiscard]] bool ping();

    /// Forget the cached connection, for example after an error.
    void disconnect();

private:
    /// Connect if there is no connection yet.
    [[nodiscard]] bool connect_socket();
    /// Send one line and read the reply, connecting first if needed.
    [[nodiscard]] Submit exchange(const std::string& line, std::uint32_t& performed);

    std::string m_path;
    int m_fd{-1};
};

} // namespace nexkey::uinput
