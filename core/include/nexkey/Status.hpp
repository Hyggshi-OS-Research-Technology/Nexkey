// SPDX-License-Identifier: MIT
// NexKey - runtime status shared between the input method addon and the GUI.

#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>

namespace nexkey {

/// Snapshot written by the input method addon and read by the diagnostics page.
struct RuntimeStatus {
    /// Program name of the input context that was typed into last.
    std::string program;
    /// "direct" or "preedit".
    std::string mode;
    /// Why that mode was chosen (short human readable text).
    std::string reason;
    /// The client announces surrounding text support.
    bool surrounding_capability{false};
    /// The client actually delivered usable surrounding text.
    bool surrounding_valid{false};
    /// Direct typing mismatches seen recently for this program.
    int recent_mismatches{0};
    /// Version and build id of the code that wrote the status.
    std::string addon_version;
    std::string addon_build;
    /// Unix time of this snapshot.
    std::int64_t updated{0};
    /// Program that was switched to preedit automatically, and when.
    std::string last_learned;
    std::int64_t last_learned_at{0};
};

/// Build identifier of the NexKey core library, e.g. "Oct 26 2026 09:15:02".
/// The addon and the GUI report it so that a stale installed addon can be told
/// apart from a freshly built one.
[[nodiscard]] std::string_view build_id() noexcept;

/// `$XDG_CACHE_HOME/nexkey/status.conf`, or `~/.cache/nexkey/status.conf`.
[[nodiscard]] std::filesystem::path default_status_path();

[[nodiscard]] bool write_status(const RuntimeStatus& status, const std::filesystem::path& path);
[[nodiscard]] bool write_status(const RuntimeStatus& status);

/// Returns false when the file is missing or unreadable.
[[nodiscard]] bool read_status(const std::filesystem::path& path, RuntimeStatus& out);
[[nodiscard]] bool read_status(RuntimeStatus& out);

} // namespace nexkey
