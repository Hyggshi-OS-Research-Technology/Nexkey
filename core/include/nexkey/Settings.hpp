// SPDX-License-Identifier: MIT
// NexKey - configuration file handling.

#pragma once

#include "nexkey/Types.hpp"

#include <filesystem>
#include <string>
#include <unordered_map>
#include <vector>

namespace nexkey {

/// Persisted configuration shared by the CLI, the tray app and the addon.
struct Settings {
    Options options{};
    /// Vietnamese mode on, English mode off.
    bool enabled{true};
    /// Key combination that toggles the mode, in portable key notation.
    std::string toggle_key{"Control+Shift"};
    bool show_tray_icon{true};
    bool start_with_system{true};
    /// Time and date format strings for macro expansion ($TIME, $DATE)
    std::string time_format{"%H:%M:%S"};
    std::string date_format{"%d/%m/%Y"};
    /// Macro table (short -> expansion)
    std::unordered_map<std::string, std::string> macros{};
    /// App Rules (application name -> method or mode, e.g. "telex", "vni", "off")
    std::unordered_map<std::string, std::string> app_rules{};
    /// Custom Dictionary words
    std::vector<std::string> custom_dictionary{};
    /// Custom keymap preset (telex, vni, viqr, microsoft)
    std::string keymap_preset{"telex"};
};

/// `$XDG_CONFIG_HOME/nexkey/nexkey.conf`, or `~/.config/nexkey/nexkey.conf`.
[[nodiscard]] std::filesystem::path default_config_path();

/// `$XDG_CONFIG_HOME/nexkey/macros.conf`, or `~/.config/nexkey/macros.conf`.
[[nodiscard]] std::filesystem::path default_macros_path();

/// Read a configuration file. Missing or invalid entries keep their defaults.
[[nodiscard]] Settings load_settings(const std::filesystem::path& path);

/// Read the user configuration, falling back to the built-in defaults.
[[nodiscard]] Settings load_settings();

/// Write a configuration file, creating parent directories when needed.
[[nodiscard]] bool save_settings(const Settings& settings, const std::filesystem::path& path);

/// Write the user configuration.
[[nodiscard]] bool save_settings(const Settings& settings);

/// Load macros from file.
[[nodiscard]] std::unordered_map<std::string, std::string> load_macros(const std::filesystem::path& path);
[[nodiscard]] std::unordered_map<std::string, std::string> load_macros();

/// Save macros to file.
[[nodiscard]] bool save_macros(const std::unordered_map<std::string, std::string>& macros, const std::filesystem::path& path);
[[nodiscard]] bool save_macros(const std::unordered_map<std::string, std::string>& macros);

/// Export configuration and macros as a JSON string.
[[nodiscard]] std::string export_backup_json(const Settings& settings);

/// Export configuration and macros to a JSON file.
[[nodiscard]] bool export_backup_file(const std::filesystem::path& path, const Settings& settings);

/// Import configuration and macros from a JSON file.
[[nodiscard]] bool import_backup_file(const std::filesystem::path& path, Settings& out_settings);

} // namespace nexkey

