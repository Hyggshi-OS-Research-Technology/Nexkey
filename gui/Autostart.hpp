// SPDX-License-Identifier: MIT
// NexKey - autostart entry handling.

#pragma once

namespace nexkey::gui {

/// Apply the "start with the session" setting.
///
/// A distribution package installs a system wide entry in
/// `/etc/xdg/autostart`, which is disabled per user with a `Hidden=true`
/// override. When no system wide entry exists (AppImage, manual install) a
/// normal per user entry is written instead.
void apply_autostart(bool enabled);

/// True when an entry that starts the tray icon is in place.
[[nodiscard]] bool autostart_installed();

} // namespace nexkey::gui
