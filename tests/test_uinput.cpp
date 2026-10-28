// SPDX-License-Identifier: MIT
// The virtual keyboard helper: wire format, timing plan and the client.

#include "TestHarness.hpp"

#include "nexkey/Replacement.hpp"
#include "nexkey/Uinput.hpp"
#include "nexkey/UinputClient.hpp"

#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <string>
#include <string_view>
#include <thread>
#include <utility>

#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

using nexkey::uinput::Client;
using nexkey::uinput::Delays;
using nexkey::uinput::Event;
using nexkey::uinput::Request;
using nexkey::uinput::Submit;

namespace {

/// A stand-in for the helper: listens on a socket and answers with whatever the
/// test told it to, so the client is exercised over a real connection.
class FakeHelper {
public:
    explicit FakeHelper(std::string reply)
        : m_reply(std::move(reply))
    {
        m_path = "/tmp/nexkey-test-uinput-" + std::to_string(::getpid()) + "-" +
                 std::to_string(++s_counter) + ".sock";
        m_fd = ::socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0);
        if (m_fd < 0) {
            return;
        }
        struct sockaddr_un address {};
        address.sun_family = AF_UNIX;
        std::memcpy(address.sun_path, m_path.c_str(), m_path.size() + 1);
        ::unlink(m_path.c_str());
        if (::bind(m_fd, reinterpret_cast<struct sockaddr*>(&address), sizeof(address)) != 0 ||
            ::listen(m_fd, 4) != 0) {
            ::close(m_fd);
            m_fd = -1;
            return;
        }
        m_thread = std::thread([this] { serve(); });
    }

    ~FakeHelper()
    {
        m_stop = true;
        // Closing the listening socket is what unblocks accept(); sending a
        // connection to ourselves is the portable way to do it from a test.
        if (m_fd >= 0) {
            static_cast<void>(::shutdown(m_fd, SHUT_RDWR));
            ::close(m_fd);
        }
        if (m_thread.joinable()) {
            m_thread.join();
        }
        if (!m_path.empty()) {
            ::unlink(m_path.c_str());
        }
    }

    FakeHelper(const FakeHelper&) = delete;
    FakeHelper& operator=(const FakeHelper&) = delete;

    [[nodiscard]] const std::string& path() const { return m_path; }
    [[nodiscard]] std::string last_request() const { return m_last_line; }

private:
    void serve()
    {
        while (!m_stop && m_fd >= 0) {
            const int client = ::accept(m_fd, nullptr, nullptr);
            if (client < 0) {
                return;
            }
            std::string line;
            char character = 0;
            while (::read(client, &character, 1) == 1 && character != '\n') {
                line.push_back(character);
            }
            m_last_line = line;
            if (!m_reply.empty()) {
                static_cast<void>(::write(client, m_reply.data(), m_reply.size()));
            }
            ::close(client);
        }
    }

    static inline std::atomic<unsigned> s_counter{0};
    std::string m_path;
    std::string m_reply;
    int m_fd{-1};
    std::atomic<bool> m_stop{false};
    std::thread m_thread;
    std::string m_last_line;
};

constexpr Delays kDefaultDelays{10, 4, 6};

} // namespace

TEST(uinput_request_round_trip)
{
    const Request request{3, kDefaultDelays};
    const auto line = nexkey::uinput::encode_request(request);
    EXPECT_EQ(line.back(), '\n');

    const auto parsed = nexkey::uinput::parse_request(line);
    EXPECT_TRUE(parsed.has_value());
    EXPECT_EQ(*parsed, request);

    // The line has to stay small enough for one write.
    EXPECT_TRUE(line.size() < 64);
}

TEST(uinput_request_rejects_nonsense)
{
    EXPECT_FALSE(nexkey::uinput::parse_request("").has_value());
    EXPECT_FALSE(nexkey::uinput::parse_request("hello").has_value());
    EXPECT_FALSE(nexkey::uinput::parse_request("NEXKEY-UINPUT 2 DEL 1 10 4 6").has_value());
    EXPECT_FALSE(nexkey::uinput::parse_request("NEXKEY-UINPUT 1 KEY 30 10 4 6").has_value());
    // Numbers that are not numbers.
    EXPECT_FALSE(nexkey::uinput::parse_request("NEXKEY-UINPUT 1 DEL x 10 4 6").has_value());
    EXPECT_FALSE(nexkey::uinput::parse_request("NEXKEY-UINPUT 1 DEL -1 10 4 6").has_value());
    EXPECT_FALSE(nexkey::uinput::parse_request("NEXKEY-UINPUT 1 DEL 1 10 4").has_value());
    EXPECT_FALSE(nexkey::uinput::parse_request("NEXKEY-UINPUT 1 DEL 1 10 4 6 7").has_value());
}

TEST(uinput_request_limits)
{
    // The helper is not a way to hold a key down forever.
    EXPECT_FALSE(nexkey::uinput::parse_request("NEXKEY-UINPUT 1 DEL 65 10 4 6").has_value());
    EXPECT_TRUE(nexkey::uinput::parse_request("NEXKEY-UINPUT 1 DEL 64 10 4 6").has_value());
    // ...nor to make it sleep for a second per character.
    EXPECT_FALSE(nexkey::uinput::parse_request("NEXKEY-UINPUT 1 DEL 1 201 4 6").has_value());
    EXPECT_FALSE(nexkey::uinput::parse_request("NEXKEY-UINPUT 1 DEL 1 10 201 6").has_value());
    EXPECT_TRUE(nexkey::uinput::parse_request("NEXKEY-UINPUT 1 DEL 1 200 200 200").has_value());
}

TEST(uinput_replies)
{
    std::uint32_t count = 0;
    EXPECT_TRUE(nexkey::uinput::parse_ok(nexkey::uinput::encode_ok(3), count));
    EXPECT_EQ(count, 3u);
    // A trailing newline is optional: a client may read the line without it.
    EXPECT_TRUE(nexkey::uinput::parse_ok("OK 7", count));
    EXPECT_EQ(count, 7u);

    EXPECT_FALSE(nexkey::uinput::parse_ok("ERR busy", count));
    EXPECT_FALSE(nexkey::uinput::parse_ok("OK", count));
    EXPECT_FALSE(nexkey::uinput::parse_ok("OK x", count));

    EXPECT_EQ(nexkey::uinput::parse_error("ERR device failed\n"), std::string_view("device failed"));
    EXPECT_TRUE(nexkey::uinput::parse_error("OK 1").empty());

    // An error reason with a newline cannot break the framing.
    const auto wrapped = nexkey::uinput::encode_error("two\nlines");
    EXPECT_EQ(wrapped, std::string("ERR two lines\n"));
}

TEST(uinput_only_the_same_user_may_drive_the_helper)
{
    EXPECT_TRUE(nexkey::uinput::trusted_peer(1000, 1000));
    EXPECT_FALSE(nexkey::uinput::trusted_peer(1001, 1000));
    EXPECT_FALSE(nexkey::uinput::trusted_peer(0, 1000));
    // Root never runs an input method, so a root client is refused as well.
    EXPECT_FALSE(nexkey::uinput::trusted_peer(0, 0));
}

TEST(uinput_sequence_presses_backspace_then_the_barrier)
{
    const auto events = nexkey::uinput::plan_sequence(Request{2, kDefaultDelays});

    // Each key is press, sync, release, sync: two backspaces and the barrier.
    EXPECT_EQ(events.size(), std::size_t{12});

    // First backspace, after the pre delay.
    EXPECT_EQ(events[0], (Event{nexkey::uinput::kEvKey, nexkey::uinput::kKeyBackspace, 1, 10}));
    EXPECT_EQ(events[1], (Event{nexkey::uinput::kEvSyn, nexkey::uinput::kSynReport, 0, 0}));
    EXPECT_EQ(events[2], (Event{nexkey::uinput::kEvKey, nexkey::uinput::kKeyBackspace, 0, 0}));

    // Second backspace, after the interval.
    EXPECT_EQ(events[4], (Event{nexkey::uinput::kEvKey, nexkey::uinput::kKeyBackspace, 1, 4}));

    // The barrier comes last, after the post delay, and is a pause.
    EXPECT_EQ(events[8], (Event{nexkey::uinput::kEvKey, nexkey::uinput::kKeyPause, 1, 6}));
    EXPECT_EQ(events[10], (Event{nexkey::uinput::kEvKey, nexkey::uinput::kKeyPause, 0, 0}));

    // Presses and releases alternate, so no key is left down.
    for (std::size_t index = 0; index + 2 < events.size(); index += 4) {
        EXPECT_EQ(events[index].value, 1);
        EXPECT_EQ(events[index + 2].value, 0);
        EXPECT_EQ(events[index].code, events[index + 2].code);
    }
}

TEST(uinput_sequence_without_backspaces_is_only_the_barrier)
{
    const auto events = nexkey::uinput::plan_sequence(Request{0, kDefaultDelays});
    EXPECT_EQ(events.size(), std::size_t{4});
    EXPECT_EQ(events[0].code, nexkey::uinput::kKeyPause);
}

TEST(uinput_total_duration_matches_the_plan)
{
    EXPECT_EQ(nexkey::uinput::total_duration_ms(Request{0, Delays{10, 4, 6}}), 10u);
    EXPECT_EQ(nexkey::uinput::total_duration_ms(Request{1, Delays{10, 4, 6}}), 16u);
    EXPECT_EQ(nexkey::uinput::total_duration_ms(Request{3, Delays{10, 4, 6}}), 24u);
    // The device waits are short on purpose: a burst must stay under the
    // deadline the frontend allows for it.
    EXPECT_TRUE(nexkey::uinput::total_duration_ms(Request{8, Delays{10, 4, 6}}) <
                nexkey::ReplacementController::kDefaultDeadlineMs);
}

TEST(uinput_client_submits_a_burst)
{
    FakeHelper helper{nexkey::uinput::encode_ok(3)};
    EXPECT_TRUE(::access(helper.path().c_str(), F_OK) == 0);

    Client client{helper.path()};
    std::uint32_t performed = 0;
    EXPECT_TRUE(client.submit(Request{3, kDefaultDelays}, performed) == Submit::Sent);
    EXPECT_EQ(performed, 3u);

    // The helper saw exactly the request the client meant to send.
    const auto parsed = nexkey::uinput::parse_request(helper.last_request());
    EXPECT_TRUE(parsed.has_value());
    EXPECT_EQ(*parsed, (Request{3, kDefaultDelays}));
}

TEST(uinput_client_reports_a_missing_helper)
{
    Client client{"/tmp/nexkey-there-is-no-such-helper.sock"};
    std::uint32_t performed = 0;
    EXPECT_TRUE(client.submit(Request{2, kDefaultDelays}, performed) == Submit::NoHelper);
    EXPECT_EQ(performed, 0u);
    EXPECT_FALSE(client.ping());
}

TEST(uinput_client_reports_a_refusal)
{
    FakeHelper helper{nexkey::uinput::encode_error("device failed")};
    Client client{helper.path()};
    std::uint32_t performed = 0;
    EXPECT_TRUE(client.submit(Request{2, kDefaultDelays}, performed) == Submit::Refused);
}

TEST(uinput_client_reports_a_broken_connection)
{
    // A listener that never answers: the client must give up quickly instead of
    // stalling the keystroke that is waiting for it.
    FakeHelper helper("");
    Client client{helper.path()};
    std::uint32_t performed = 0;
    const auto start = std::chrono::steady_clock::now();
    const auto outcome = client.submit(Request{2, kDefaultDelays}, performed);
    const auto elapsed = std::chrono::steady_clock::now() - start;

    EXPECT_TRUE(outcome == Submit::Failed);
    EXPECT_TRUE(elapsed < std::chrono::milliseconds(500));
}

TEST(uinput_client_refuses_an_impossible_request)
{
    Client client{"/tmp/nexkey-never-used.sock"};
    std::uint32_t performed = 0;
    // Nothing is sent for a request the helper would reject anyway, so a bug in
    // the caller cannot reach the device.
    EXPECT_TRUE(client.submit(Request{nexkey::uinput::kMaxBackspaces + 1, kDefaultDelays}, performed) ==
                Submit::Refused);
    EXPECT_TRUE(client.submit(Request{1, Delays{0, 0, 500}}, performed) == Submit::Refused);
}

TEST(uinput_client_pings_the_helper)
{
    FakeHelper helper{"PONG\n"};
    Client client{helper.path()};
    EXPECT_TRUE(client.ping());
    EXPECT_EQ(helper.last_request(), std::string("PING"));
}
