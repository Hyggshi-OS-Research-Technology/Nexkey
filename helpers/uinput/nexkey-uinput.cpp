// SPDX-License-Identifier: MIT
// NexKey - virtual keyboard helper.
//
// This program owns the only two things that need privileges or kernel access
// in NexKey: a listening socket under the user's runtime directory, and a
// virtual keyboard device created through /dev/uinput. The input method addon
// asks it to press BACKSPACE a number of times; nothing else can be asked of
// it, because the protocol has no field for a key code.
//
// Why a separate process at all:
//   * /dev/uinput is usually root-only or needs a udev rule; the addon runs
//     inside Fcitx5 and should not hold that handle.
//   * The device has to survive a restart of Fcitx5, or every restart would
//     leave a dead virtual keyboard behind.
//   * A crash in the writer cannot take the input method with it.
//
// What it is not: it cannot type text, it cannot read the keyboard, and it
// refuses every client that is not the user who started it. The kernel makes
// the same point from the other side: a uinput device can only inject, never
// observe.

#include "nexkey/Uinput.hpp"

#include <cerrno>
#include <csignal>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <string_view>
#include <vector>

#include <fcntl.h>
#include <poll.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <unistd.h>

#include <linux/input.h>
#include <linux/uinput.h>

namespace {

/// Largest line the helper accepts, so a client cannot make it grow without
/// bound. A request is under 60 bytes.
constexpr std::size_t kMaxLine = 256;

volatile std::sig_atomic_t g_stop = 0;

void on_signal(int) { g_stop = 1; }

/// Sleep for `milliseconds`, waking up early when the helper has to stop.
void wait_ms(std::uint32_t milliseconds)
{
    if (milliseconds == 0 || g_stop != 0) {
        return;
    }
    // poll() with no descriptors is the portable way to sleep in a program that
    // already has one event loop, and it returns as soon as a signal arrives.
    static_cast<void>(::poll(nullptr, 0, static_cast<int>(milliseconds)));
}

class UniqueFd {
public:
    UniqueFd() = default;
    explicit UniqueFd(int fd)
        : m_fd(fd)
    {
    }
    ~UniqueFd()
    {
        if (m_fd >= 0) {
            ::close(m_fd);
        }
    }
    UniqueFd(const UniqueFd&) = delete;
    UniqueFd& operator=(const UniqueFd&) = delete;
    UniqueFd(UniqueFd&& other) noexcept
        : m_fd(other.m_fd)
    {
        other.m_fd = -1;
    }
    UniqueFd& operator=(UniqueFd&& other) noexcept
    {
        if (this != &other) {
            if (m_fd >= 0) {
                ::close(m_fd);
            }
            m_fd = other.m_fd;
            other.m_fd = -1;
        }
        return *this;
    }

    [[nodiscard]] int get() const noexcept { return m_fd; }
    [[nodiscard]] bool valid() const noexcept { return m_fd >= 0; }
    int release() noexcept
    {
        const int fd = m_fd;
        m_fd = -1;
        return fd;
    }

private:
    int m_fd{-1};
};

/// The virtual keyboard. Created once, destroyed on exit.
class VirtualKeyboard {
public:
    /// Returns false and sets `reason` when the device cannot be created, which
    /// is the normal situation for a user who is not in the right group.
    bool open(std::string& reason)
    {
        m_fd = UniqueFd(::open("/dev/uinput", O_WRONLY | O_NONBLOCK | O_CLOEXEC));
        if (!m_fd.valid()) {
            reason = std::string("cannot open /dev/uinput: ") + std::strerror(errno);
            return false;
        }

        // Only the two keys the burst uses. Granting more would turn the helper
        // into something worth attacking.
        if (::ioctl(m_fd.get(), UI_SET_EVBIT, EV_KEY) < 0) {
            reason = std::string("UI_SET_EVBIT: ") + std::strerror(errno);
            return false;
        }
        for (const int key : {KEY_BACKSPACE, KEY_PAUSE}) {
            if (::ioctl(m_fd.get(), UI_SET_KEYBIT, key) < 0) {
                reason = std::string("UI_SET_KEYBIT: ") + std::strerror(errno);
                return false;
            }
        }

        struct uinput_setup setup {};
        setup.id.bustype = BUS_USB;
        setup.id.vendor = 0x4E4B;  // 'NK'
        setup.id.product = 0x0001;
        setup.id.version = 1;
        std::snprintf(setup.name, sizeof(setup.name), "NexKey virtual keyboard");
        if (::ioctl(m_fd.get(), UI_DEV_SETUP, &setup) < 0) {
            reason = std::string("UI_DEV_SETUP: ") + std::strerror(errno);
            return false;
        }
        if (::ioctl(m_fd.get(), UI_DEV_CREATE) < 0) {
            reason = std::string("UI_DEV_CREATE: ") + std::strerror(errno);
            return false;
        }
        m_created = true;

        // The device appears asynchronously in the input layer. Events written
        // before the seat picked it up are lost, which shows up as the first
        // word of a session losing a letter.
        wait_ms(30);
        return true;
    }

    ~VirtualKeyboard()
    {
        if (m_created && m_fd.valid()) {
            static_cast<void>(::ioctl(m_fd.get(), UI_DEV_DESTROY));
        }
    }

    // The device must not be copied: two objects would destroy it twice.
    VirtualKeyboard() = default;
    VirtualKeyboard(const VirtualKeyboard&) = delete;
    VirtualKeyboard& operator=(const VirtualKeyboard&) = delete;

    /// Write one planned event.
    bool send(const nexkey::uinput::Event& event)
    {
        struct input_event native {};
        native.type = event.type;
        native.code = event.code;
        native.value = event.value;
        const auto* bytes = reinterpret_cast<const char*>(&native);
        std::size_t written = 0;
        while (written < sizeof(native)) {
            const ssize_t result = ::write(m_fd.get(), bytes + written, sizeof(native) - written);
            if (result > 0) {
                written += static_cast<std::size_t>(result);
                continue;
            }
            if (result < 0 && errno == EINTR) {
                continue;
            }
            return false;
        }
        return true;
    }

    [[nodiscard]] bool valid() const noexcept { return m_created; }

private:
    UniqueFd m_fd;
    bool m_created{false};
};

/// Play one request. Returns false when the device is gone.
bool play(VirtualKeyboard& keyboard, const nexkey::uinput::Request& request)
{
    for (const auto& event : nexkey::uinput::plan_sequence(request)) {
        if (g_stop != 0) {
            return false;
        }
        wait_ms(event.wait_before_ms);
        if (!keyboard.send(event)) {
            return false;
        }
    }
    // The kernel needs a moment to deliver the last events to the seat before
    // the next request starts, otherwise two bursts can arrive interleaved.
    wait_ms(1);
    return true;
}

bool peer_is_ours(int fd)
{
#if defined(SO_PEERCRED)
    struct ucred credentials {};
    socklen_t length = sizeof(credentials);
    if (::getsockopt(fd, SOL_SOCKET, SO_PEERCRED, &credentials, &length) != 0) {
        return false;
    }
    return nexkey::uinput::trusted_peer(static_cast<std::uint32_t>(credentials.uid),
                                        static_cast<std::uint32_t>(::getuid()));
#else
    static_cast<void>(fd);
    return false;
#endif
}

/// Read one line from `fd`. Returns false when the connection is gone.
bool read_line(int fd, std::string& line)
{
    line.clear();
    while (line.size() < kMaxLine) {
        char character = 0;
        const ssize_t result = ::read(fd, &character, 1);
        if (result == 0) {
            return false;
        }
        if (result < 0) {
            if (errno == EINTR) {
                continue;
            }
            // The socket is blocking, so anything else is a broken connection.
            return false;
        }
        if (character == '\n') {
            return true;
        }
        line.push_back(character);
    }
    return false;
}

bool write_all(int fd, std::string_view text)
{
    std::size_t written = 0;
    while (written < text.size()) {
        const ssize_t result = ::write(fd, text.data() + written, text.size() - written);
        if (result <= 0) {
            if (result < 0 && errno == EINTR) {
                continue;
            }
            return false;
        }
        written += static_cast<std::size_t>(result);
    }
    return true;
}

int make_socket(const std::string& path, std::string& reason)
{
    // The socket lives in a directory only the user can enter, so the file mode
    // alone is not what protects it.
    const auto slash = path.find_last_of('/');
    if (slash != std::string::npos) {
        const auto directory = path.substr(0, slash);
        if (::mkdir(directory.c_str(), 0700) != 0 && errno != EEXIST) {
            reason = "cannot create " + directory + ": " + std::strerror(errno);
            return -1;
        }
        struct stat info {};
        if (::stat(directory.c_str(), &info) != 0 || info.st_uid != ::getuid()) {
            reason = directory + " is not owned by this user";
            return -1;
        }
    }

    UniqueFd socket(::socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0));
    if (!socket.valid()) {
        reason = std::string("socket: ") + std::strerror(errno);
        return -1;
    }

    struct sockaddr_un address {};
    address.sun_family = AF_UNIX;
    if (path.size() >= sizeof(address.sun_path)) {
        reason = "socket path is too long: " + path;
        return -1;
    }
    std::memcpy(address.sun_path, path.c_str(), path.size() + 1);

    // A leftover socket from a helper that was killed would make bind() fail
    // forever, so it is removed first. The directory check above is what makes
    // that safe: nobody else can have put a file there.
    static_cast<void>(::unlink(path.c_str()));

    if (::bind(socket.get(), reinterpret_cast<struct sockaddr*>(&address), sizeof(address)) != 0) {
        reason = std::string("bind: ") + std::strerror(errno);
        return -1;
    }
    if (::chmod(path.c_str(), 0600) != 0) {
        reason = std::string("chmod: ") + std::strerror(errno);
        return -1;
    }
    if (::listen(socket.get(), 4) != 0) {
        reason = std::string("listen: ") + std::strerror(errno);
        return -1;
    }
    return socket.release();
}

void usage(const char* program)
{
    std::printf("usage: %s [--socket PATH] [--dry-run] [--verbose]\n\n"
                "Virtual keyboard helper for NexKey. The input method asks it to press\n"
                "BACKSPACE a few times through /dev/uinput, so that applications which\n"
                "ignore synthetic key events still delete what they should.\n\n"
                "  --socket PATH  socket to listen on (default: $XDG_RUNTIME_DIR/nexkey/uinput.sock)\n"
                "  --dry-run      accept requests and report them without a device, for testing\n"
                "  --verbose      print every request\n",
                program);
}

} // namespace

int main(int argc, char** argv)
{
    std::string socket_path = nexkey::uinput::default_socket_path();
    bool dry_run = false;
    bool verbose = false;

    for (int index = 1; index < argc; ++index) {
        const std::string_view argument{argv[index]};
        if (argument == "--help" || argument == "-h") {
            usage(argv[0]);
            return 0;
        }
        if (argument == "--dry-run") {
            dry_run = true;
        } else if (argument == "--verbose") {
            verbose = true;
        } else if (argument == "--socket" && index + 1 < argc) {
            socket_path = argv[++index];
        } else {
            std::fprintf(stderr, "nexkey-uinput: unknown argument %s\n", argv[index]);
            return 2;
        }
    }

    if (socket_path.empty()) {
        std::fprintf(stderr,
                     "nexkey-uinput: no socket path (XDG_RUNTIME_DIR is not set, pass --socket)\n");
        return 2;
    }

    std::signal(SIGINT, on_signal);
    std::signal(SIGTERM, on_signal);
    std::signal(SIGPIPE, SIG_IGN);

    VirtualKeyboard keyboard;
    if (!dry_run) {
        std::string reason;
        if (!keyboard.open(reason)) {
            // Without a device the helper is useless, but saying why is the
            // whole value of this program: the message goes to the user's log.
            std::fprintf(stderr, "nexkey-uinput: %s\n", reason.c_str());
            std::fprintf(stderr,
                         "nexkey-uinput: add a udev rule for /dev/uinput or run the helper as a "
                         "user in the uinput group\n");
            return 1;
        }
    }

    std::string reason;
    UniqueFd listener(make_socket(socket_path, reason));
    if (!listener.valid()) {
        std::fprintf(stderr, "nexkey-uinput: %s\n", reason.c_str());
        return 1;
    }

    if (verbose) {
        std::fprintf(stderr, "nexkey-uinput: listening on %s%s\n", socket_path.c_str(),
                     dry_run ? " (dry run)" : "");
    }

    while (g_stop == 0) {
        struct pollfd wait {};
        wait.fd = listener.get();
        wait.events = POLLIN;
        const int ready = ::poll(&wait, 1, 500);
        if (ready < 0) {
            if (errno == EINTR) {
                continue;
            }
            break;
        }
        if (ready == 0) {
            continue;
        }

        UniqueFd client(::accept4(listener.get(), nullptr, nullptr, SOCK_CLOEXEC));
        if (!client.valid()) {
            continue;
        }
        if (!peer_is_ours(client.get())) {
            static_cast<void>(write_all(client.get(), nexkey::uinput::encode_error("not your helper")));
            continue;
        }

        std::string line;
        while (g_stop == 0 && read_line(client.get(), line)) {
            if (line == nexkey::uinput::kPing) {
                static_cast<void>(write_all(client.get(), nexkey::uinput::kPong));
                static_cast<void>(write_all(client.get(), "\n"));
                continue;
            }

            const auto request = nexkey::uinput::parse_request(line);
            if (!request) {
                static_cast<void>(write_all(client.get(), nexkey::uinput::encode_error("bad request")));
                continue;
            }

            if (dry_run) {
                static_cast<void>(write_all(
                    client.get(), nexkey::uinput::encode_ok(request->backspaces)));
                if (verbose) {
                    std::fprintf(stderr, "nexkey-uinput: dry run, %u backspaces\n",
                                 request->backspaces);
                }
                continue;
            }

            if (!play(keyboard, *request)) {
                // The device is gone; nothing good can come of carrying on with
                // a virtual keyboard that no longer exists.
                static_cast<void>(write_all(client.get(), nexkey::uinput::encode_error("device failed")));
                g_stop = 1;
                break;
            }
            static_cast<void>(write_all(client.get(), nexkey::uinput::encode_ok(request->backspaces)));
            if (verbose) {
                std::fprintf(stderr, "nexkey-uinput: %u backspaces\n", request->backspaces);
            }
        }
    }

    static_cast<void>(::unlink(socket_path.c_str()));
    return 0;
}
