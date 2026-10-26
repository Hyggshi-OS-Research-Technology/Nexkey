// SPDX-License-Identifier: MIT
// NexKey - environment and runtime diagnostics for the settings dialog.

#pragma once

#include <QString>
#include <QStringList>

namespace nexkey::gui {

struct DiagnosticsReport {
    /// Full plain-text report, suitable for copying into a bug report.
    QString text;
    /// One line per detected problem; empty when everything looks fine.
    QStringList issues;
};

/// Inspect the session, fcitx5, the installed addon copies, the GUI binaries on
/// PATH and the status snapshot written by the addon.
[[nodiscard]] DiagnosticsReport collect_diagnostics();

} // namespace nexkey::gui
