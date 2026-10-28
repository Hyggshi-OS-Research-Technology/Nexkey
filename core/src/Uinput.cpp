// SPDX-License-Identifier: MIT
// NexKey - protocol and timing plan for the virtual keyboard helper.

#include "nexkey/Uinput.hpp"

#include "nexkey/Unicode.hpp"

#include <cstdlib>
#include <string>
#include <string_view>

#if defined(__unix__)
#include <unistd.h>
#endif

namespace nexkey::uinput {

namespace {

constexpr std::string_view kPrefix{"NEXKEY-UINPUT"};
constexpr std::string_view kVersion{"1"};
constexpr std::string_view kDelete{"DEL"};

/// Parse an unsigned decimal, refusing anything that is not one.
bool parse_number(std::string_view text, std::uint32_t& out) noexcept
{
    if (text.empty() || text.size() > 9) {
        return false;
    }
    std::uint32_t value = 0;
    for (const char character : text) {
        if (character < '0' || character > '9') {
            return false;
        }
        value = value * 10U + static_cast<std::uint32_t>(character - '0');
    }
    out = value;
    return true;
}

std::vector<std::string_view> split(std::string_view line) noexcept
{
    std::vector<std::string_view> parts;
    std::size_t start = 0;
    while (start <= line.size()) {
        const auto end = line.find(' ', start);
        parts.push_back(line.substr(start, end == std::string_view::npos ? std::string_view::npos
                                                                       : end - start));
        if (end == std::string_view::npos) {
            break;
        }
        start = end + 1;
    }
    return parts;
}

std::string_view trim(std::string_view line) noexcept
{
    while (!line.empty() && (line.back() == '\n' || line.back() == '\r')) {
        line.remove_suffix(1);
    }
    return line;
}

/// The press and release of one key, in the order the kernel expects.
void append_click(std::vector<Event>& events, std::uint16_t code, std::uint32_t wait_before_ms)
{
    events.push_back(Event{kEvKey, code, 1, wait_before_ms});
    events.push_back(Event{kEvSyn, kSynReport, 0, 0});
    events.push_back(Event{kEvKey, code, 0, 0});
    events.push_back(Event{kEvSyn, kSynReport, 0, 0});
}

} // namespace

std::string default_socket_path()
{
    if (const char* runtime = std::getenv("XDG_RUNTIME_DIR"); runtime != nullptr && *runtime != '\0') {
        return std::string(runtime) + "/nexkey/uinput.sock";
    }
    // systemd and elogind both create /run/user/<uid>; without it the socket
    // has nowhere private to live.
#if defined(__unix__)
    return std::string("/run/user/") + std::to_string(::getuid()) + "/nexkey/uinput.sock";
#endif
    // Without a runtime directory there is nowhere private to put a socket, and
    // a shared /tmp path would let another user impersonate the helper.
    return {};
}

std::string encode_request(const Request& request)
{
    std::string line(kPrefix);
    line += ' ';
    line += kVersion;
    line += ' ';
    line += kDelete;
    line += ' ';
    line += std::to_string(request.backspaces);
    line += ' ';
    line += std::to_string(request.delays.pre_ms);
    line += ' ';
    line += std::to_string(request.delays.interval_ms);
    line += ' ';
    line += std::to_string(request.delays.post_ms);
    line += '\n';
    return line;
}

std::optional<Request> parse_request(std::string_view line)
{
    const auto parts = split(trim(line));
    // "NEXKEY-UINPUT" "1" "DEL" n pre interval post
    if (parts.size() != 7 || parts[0] != kPrefix || parts[1] != kVersion || parts[2] != kDelete) {
        return std::nullopt;
    }

    Request request;
    if (!parse_number(parts[3], request.backspaces) || !parse_number(parts[4], request.delays.pre_ms) ||
        !parse_number(parts[5], request.delays.interval_ms) ||
        !parse_number(parts[6], request.delays.post_ms)) {
        return std::nullopt;
    }
    if (request.backspaces > kMaxBackspaces || !request.delays.valid()) {
        return std::nullopt;
    }
    return request;
}

std::string encode_ok(std::uint32_t backspaces)
{
    return "OK " + std::to_string(backspaces) + "\n";
}

std::string encode_error(std::string_view reason)
{
    std::string line{"ERR "};
    for (const char character : reason) {
        line += (character == '\n' || character == '\r') ? ' ' : character;
    }
    line += '\n';
    return line;
}

bool parse_ok(std::string_view line, std::uint32_t& backspaces)
{
    const auto parts = split(trim(line));
    if (parts.size() != 2 || parts[0] != "OK") {
        return false;
    }
    return parse_number(parts[1], backspaces);
}

std::string_view parse_error(std::string_view line)
{
    const auto trimmed = trim(line);
    if (!trimmed.starts_with("ERR ")) {
        return {};
    }
    return trimmed.substr(4);
}

bool trusted_peer(std::uint32_t peer_uid, std::uint32_t our_uid) noexcept
{
    // Only the same user may drive the helper. Root is refused as well: root
    // never runs an input method, so a root client is either a mistake or an
    // attempt to be let through.
    return peer_uid == our_uid && our_uid != 0;
}

std::vector<Event> plan_sequence(const Request& request)
{
    std::vector<Event> events;
    events.reserve(static_cast<std::size_t>(request.backspaces) * 4U + 4U);

    for (std::uint32_t press = 0; press < request.backspaces; ++press) {
        // The first press waits for the application to finish whatever it was
        // doing; every later one keeps a steady pace so that the queue does not
        // fill up and start dropping events.
        append_click(events, kKeyBackspace, press == 0 ? request.delays.pre_ms : request.delays.interval_ms);
    }

    // The barrier: the last release gets its own pause, then the key that tells
    // the frontend the burst is over.
    append_click(events, kKeyPause, request.backspaces == 0 ? request.delays.pre_ms : request.delays.post_ms);
    return events;
}

std::uint32_t total_duration_ms(const Request& request)
{
    // Summing the plan is slower than a formula and cannot disagree with it.
    std::uint32_t total = 0;
    for (const auto& event : plan_sequence(request)) {
        total += event.wait_before_ms;
    }
    return total;
}

} // namespace nexkey::uinput
