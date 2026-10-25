// SPDX-License-Identifier: MIT
// NexKey - configuration file handling.

#pragma once

#include "nexkey/Types.hpp"

#include <filesystem>
#include <string>

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
};

/// `$XDG_CONFIG_HOME/nexkey/nexkey.conf`, or `~/.config/nexkey/nexkey.conf`.
[[nodiscard]] std::filesystem::path default_config_path();

/// Read a configuration file. Missing or invalid entries keep their defaults.
[[nodiscard]] Settings load_settings(const std::filesystem::path& path);

/// Read the user configuration, falling back to the built-in defaults.
[[nodiscard]] Settings load_settings();

/// Write a configuration file, creating parent directories when needed.
[[nodiscard]] bool save_settings(const Settings& settings, const std::filesystem::path& path);

/// Write the user configuration.
[[nodiscard]] bool save_settings(const Settings& settings);

} // namespace nexkey
