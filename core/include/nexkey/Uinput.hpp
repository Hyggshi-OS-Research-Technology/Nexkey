// SPDX-License-Identifier: MIT
// NexKey - the delete burst that goes through a virtual keyboard device.
//
// Direct typing has to delete characters before it types new ones, and some
// applications ignore every request the input method can make: they accept
// deleteSurroundingText and do nothing, and they treat a forwarded key as a
// synthetic event that never happened. Those applications do honour one thing:
// a real key press coming from a real keyboard device.
//
// So NexKey asks a small helper process to create a virtual keyboard through
// /dev/uinput and to press BACKSPACE a few times. The events travel through the
// kernel, arrive at Fcitx5 like any other key, and the application gets them as
// genuine keys.
//
// This header has the wire format and the timing plan. Both are pure data so
// that they can be tested without a kernel, a socket or a display.
//
// Clean room note: the *idea* of doing the deletion through a virtual keyboard
// is how the GPL-3 licensed fcitx5-lotus solves the same problem, and NexKey
// follows it deliberately. No line of that project was read or copied while
// writing this; the protocol below, the timing plan, the helper and the
// bookkeeping are NexKey's own, and the whole subtree stays MIT.

#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace nexkey::uinput {

/// Path of the socket the helper listens on, under the per user runtime
/// directory so that no other user can reach it.
[[nodiscard]] std::string default_socket_path();

/// Hard limits of the protocol. A word never needs more deletions than this,
/// and the helper must not be usable as a way to hold a key down forever.
inline constexpr std::uint32_t kMaxBackspaces = 64;
inline constexpr std::uint32_t kMaxDelayMs = 200;

/// How long the helper waits around the burst.
///
/// The delays are not decoration: writing all the events in one go makes some
/// toolkits drop or coalesce them, and pressing a key while another one is
/// still down confuses applications that track modifiers.
struct Delays {
    std::uint32_t pre_ms{10};      ///< Before the first press.
    std::uint32_t interval_ms{4};  ///< Between two presses.
    std::uint32_t post_ms{6};      ///< Between the last release and the barrier.

    [[nodiscard]] bool valid() const noexcept
    {
        return pre_ms <= kMaxDelayMs && interval_ms <= kMaxDelayMs && post_ms <= kMaxDelayMs;
    }

    friend constexpr bool operator==(const Delays&, const Delays&) = default;
};

/// What the frontend asks the helper to do.
///
/// There is deliberately no field for a key code: the helper can press
/// BACKSPACE and the barrier key and nothing else, so a compromised or
/// confused client cannot turn it into a general purpose keyboard injector.
struct Request {
    std::uint32_t backspaces{0};
    Delays delays{};

    friend constexpr bool operator==(const Request&, const Request&) = default;
};

/// One line of the protocol.
[[nodiscard]] std::string encode_request(const Request& request);

/// Parse a request line. Empty when the line is not a valid request.
[[nodiscard]] std::optional<Request> parse_request(std::string_view line);

/// The helper answers every request: "OK <backspaces>" or "ERR <reason>".
[[nodiscard]] std::string encode_ok(std::uint32_t backspaces);
[[nodiscard]] std::string encode_error(std::string_view reason);
[[nodiscard]] bool parse_ok(std::string_view line, std::uint32_t& backspaces);
[[nodiscard]] std::string_view parse_error(std::string_view line);

/// A "does the helper answer" request, for probing before the first word.
inline constexpr std::string_view kPing{"PING"};
inline constexpr std::string_view kPong{"PONG"};

/// The helper must only obey its own user. The uid check is separate from the
/// socket so that it can be tested.
[[nodiscard]] bool trusted_peer(std::uint32_t peer_uid, std::uint32_t our_uid) noexcept;

// ---------------------------------------------------------------------------
// The timing plan
// ---------------------------------------------------------------------------

/// Values of the Linux input event types used below (linux/input-event-codes.h).
inline constexpr std::uint16_t kEvSyn = 0x00;
inline constexpr std::uint16_t kEvKey = 0x01;
inline constexpr std::uint16_t kSynReport = 0;
inline constexpr std::uint16_t kKeyBackspace = 14;
inline constexpr std::uint16_t kKeyPause = 119;

/// One event of the burst, with the pause the helper takes before writing it.
struct Event {
    std::uint16_t type{0};
    std::uint16_t code{0};
    std::int32_t value{0};
    std::uint32_t wait_before_ms{0};

    friend constexpr bool operator==(const Event&, const Event&) = default;
};

/// The exact sequence the helper writes for one request: press and release
/// BACKSPACE `backspaces` times, then press and release the barrier key.
///
/// The barrier is what makes the whole thing safe. The frontend cannot know
/// when the kernel finished delivering the backspaces, so it waits for a key
/// that it recognises and swallows; by the time that key arrives, every
/// backspace before it has already been handed to the application. A pause is
/// used because no common application reacts to it, and because it sits at the
/// same position on every keyboard layout.
[[nodiscard]] std::vector<Event> plan_sequence(const Request& request);

/// Total time the helper spends sleeping for one request.
[[nodiscard]] std::uint32_t total_duration_ms(const Request& request);

} // namespace nexkey::uinput
