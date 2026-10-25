// SPDX-License-Identifier: MIT
// NexKey - configuration file handling.

#include "nexkey/Settings.hpp"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <fstream>
#include <optional>
#include <sstream>
#include <string_view>

namespace nexkey {

namespace {

std::string_view trim(std::string_view value)
{
    const auto first = value.find_first_not_of(" \t\r\n");
    if (first == std::string_view::npos) {
        return {};
    }
    const auto last = value.find_last_not_of(" \t\r\n");
    return value.substr(first, last - first + 1);
}

std::string to_lower(std::string_view value)
{
    std::string out(value);
    std::ranges::transform(out, out.begin(), [](unsigned char character) {
        return static_cast<char>(std::tolower(character));
    });
    return out;
}

bool parse_bool(std::string_view value, bool fallback)
{
    const auto lower = to_lower(trim(value));
    if (lower == "true" || lower == "yes" || lower == "on" || lower == "1") {
        return true;
    }
    if (lower == "false" || lower == "no" || lower == "off" || lower == "0") {
        return false;
    }
    return fallback;
}

std::string unquote(std::string_view value)
{
    if (value.size() >= 2 && value.front() == '"' && value.back() == '"') {
        return std::string(value.substr(1, value.size() - 2));
    }
    return std::string(value);
}

} // namespace

std::filesystem::path default_config_path()
{
    if (const char* xdg = std::getenv("XDG_CONFIG_HOME"); xdg != nullptr && *xdg != '\0') {
        return std::filesystem::path(xdg) / "nexkey" / "nexkey.conf";
    }
    if (const char* home = std::getenv("HOME"); home != nullptr && *home != '\0') {
        return std::filesystem::path(home) / ".config" / "nexkey" / "nexkey.conf";
    }
    return std::filesystem::path("nexkey.conf");
}

Settings load_settings(const std::filesystem::path& path)
{
    Settings settings;
    std::ifstream file(path);
    if (!file) {
        return settings;
    }

    std::string section;
    std::string line;
    while (std::getline(file, line)) {
        const auto trimmed = trim(line);
        if (trimmed.empty() || trimmed.front() == '#' || trimmed.front() == ';') {
            continue;
        }
        if (trimmed.front() == '[' && trimmed.back() == ']') {
            section = to_lower(trimmed.substr(1, trimmed.size() - 2));
            continue;
        }
        const auto separator = trimmed.find('=');
        if (separator == std::string_view::npos) {
            continue;
        }
        const auto key = to_lower(trim(trimmed.substr(0, separator)));
        const auto value = trim(trimmed.substr(separator + 1));

        if (section == "input") {
            if (key == "method") {
                if (const auto method = method_from_string(to_lower(value))) {
                    settings.options.method = *method;
                }
            } else if (key == "tone_style") {
                if (const auto style = tone_style_from_string(to_lower(value))) {
                    settings.options.tone_style = *style;
                }
            } else if (key == "enabled") {
                settings.enabled = parse_bool(value, settings.enabled);
            }
        } else if (section == "output") {
            if (key == "encoding") {
                if (const auto encoding = encoding_from_string(to_lower(value))) {
                    settings.options.encoding = *encoding;
                }
            }
        } else if (section == "engine") {
            if (key == "smart_backspace") {
                settings.options.smart_backspace = parse_bool(value, settings.options.smart_backspace);
            } else if (key == "english_fallback") {
                settings.options.english_fallback =
                    parse_bool(value, settings.options.english_fallback);
            } else if (key == "bracket_shortcuts") {
                settings.options.bracket_shortcuts =
                    parse_bool(value, settings.options.bracket_shortcuts);
            }
        } else if (section == "ui") {
            if (key == "toggle_key") {
                settings.toggle_key = unquote(value);
            } else if (key == "show_tray_icon") {
                settings.show_tray_icon = parse_bool(value, settings.show_tray_icon);
            } else if (key == "start_with_system") {
                settings.start_with_system = parse_bool(value, settings.start_with_system);
            }
        }
    }
    return settings;
}

Settings load_settings()
{
    return load_settings(default_config_path());
}

bool save_settings(const Settings& settings, const std::filesystem::path& path)
{
    std::error_code error;
    if (path.has_parent_path()) {
        std::filesystem::create_directories(path.parent_path(), error);
    }

    std::ofstream file(path, std::ios::trunc);
    if (!file) {
        return false;
    }

    file << "# NexKey configuration file.\n"
         << "# Edit with the tray application or with 'nexkey config set'.\n"
         << "\n[input]\n"
         << "method=" << to_string(settings.options.method) << '\n'
         << "tone_style=" << to_string(settings.options.tone_style) << '\n'
         << "enabled=" << (settings.enabled ? "true" : "false") << '\n'
         << "\n[output]\n"
         << "encoding=" << to_string(settings.options.encoding) << '\n'
         << "\n[engine]\n"
         << "smart_backspace=" << (settings.options.smart_backspace ? "true" : "false") << '\n'
         << "english_fallback=" << (settings.options.english_fallback ? "true" : "false") << '\n'
         << "bracket_shortcuts=" << (settings.options.bracket_shortcuts ? "true" : "false") << '\n'
         << "\n[ui]\n"
         << "toggle_key=" << settings.toggle_key << '\n'
         << "show_tray_icon=" << (settings.show_tray_icon ? "true" : "false") << '\n'
         << "start_with_system=" << (settings.start_with_system ? "true" : "false") << '\n';
    return file.good();
}

bool save_settings(const Settings& settings)
{
    return save_settings(settings, default_config_path());
}

} // namespace nexkey
