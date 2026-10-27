// SPDX-License-Identifier: MIT
// NexKey - runtime status shared between the input method addon and the GUI.

#include "nexkey/Status.hpp"

#include <cstdlib>
#include <fstream>
#include <string>
#include <system_error>

namespace nexkey {

namespace {

std::string one_line(std::string text)
{
    for (char& c : text) {
        if (c == '\n' || c == '\r') {
            c = ' ';
        }
    }
    return text;
}

bool parse_bool(const std::string& value) noexcept
{
    return value == "1" || value == "true";
}

std::int64_t parse_int(const std::string& value) noexcept
{
    try {
        return std::stoll(value);
    } catch (...) {
        return 0;
    }
}

} // namespace

std::string_view build_id() noexcept
{
    static constexpr std::string_view id{__DATE__ " " __TIME__};
    return id;
}

std::filesystem::path default_status_path()
{
    if (const char* xdg = std::getenv("XDG_CACHE_HOME"); xdg != nullptr && *xdg != '\0') {
        return std::filesystem::path(xdg) / "nexkey" / "status.conf";
    }
    if (const char* home = std::getenv("HOME"); home != nullptr && *home != '\0') {
        return std::filesystem::path(home) / ".cache" / "nexkey" / "status.conf";
    }
    return std::filesystem::path("/tmp") / "nexkey-status.conf";
}

bool write_status(const RuntimeStatus& status, const std::filesystem::path& path)
{
    std::error_code error;
    if (path.has_parent_path()) {
        std::filesystem::create_directories(path.parent_path(), error);
    }

    // Write next to the target and rename, so a reader never sees half a file.
    auto temporary = path;
    temporary += ".tmp";
    {
        std::ofstream file(temporary, std::ios::trunc);
        if (!file) {
            return false;
        }
        file << "program=" << one_line(status.program) << '\n'
             << "mode=" << one_line(status.mode) << '\n'
             << "reason=" << one_line(status.reason) << '\n'
             << "surrounding_capability=" << (status.surrounding_capability ? 1 : 0) << '\n'
             << "surrounding_valid=" << (status.surrounding_valid ? 1 : 0) << '\n'
             << "recent_mismatches=" << status.recent_mismatches << '\n'
             << "addon_version=" << one_line(status.addon_version) << '\n'
             << "addon_build=" << one_line(status.addon_build) << '\n'
             << "unicode_backend=" << one_line(status.unicode_backend) << '\n'
             << "updated=" << status.updated << '\n'
             << "last_learned=" << one_line(status.last_learned) << '\n'
             << "last_learned_at=" << status.last_learned_at << '\n';
        if (!file) {
            return false;
        }
    }
    std::filesystem::rename(temporary, path, error);
    return !error;
}

bool write_status(const RuntimeStatus& status)
{
    return write_status(status, default_status_path());
}

bool read_status(const std::filesystem::path& path, RuntimeStatus& out)
{
    std::ifstream file(path);
    if (!file) {
        return false;
    }

    RuntimeStatus result;
    std::string line;
    while (std::getline(file, line)) {
        const auto split = line.find('=');
        if (split == std::string::npos) {
            continue;
        }
        const std::string key = line.substr(0, split);
        const std::string value = line.substr(split + 1);
        if (key == "program") {
            result.program = value;
        } else if (key == "mode") {
            result.mode = value;
        } else if (key == "reason") {
            result.reason = value;
        } else if (key == "surrounding_capability") {
            result.surrounding_capability = parse_bool(value);
        } else if (key == "surrounding_valid") {
            result.surrounding_valid = parse_bool(value);
        } else if (key == "recent_mismatches") {
            result.recent_mismatches = static_cast<int>(parse_int(value));
        } else if (key == "addon_version") {
            result.addon_version = value;
        } else if (key == "unicode_backend") {
            result.unicode_backend = value;
        } else if (key == "addon_build") {
            result.addon_build = value;
        } else if (key == "updated") {
            result.updated = parse_int(value);
        } else if (key == "last_learned") {
            result.last_learned = value;
        } else if (key == "last_learned_at") {
            result.last_learned_at = parse_int(value);
        }
    }
    out = std::move(result);
    return true;
}

bool read_status(RuntimeStatus& out)
{
    return read_status(default_status_path(), out);
}

} // namespace nexkey
