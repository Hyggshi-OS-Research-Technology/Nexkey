// SPDX-License-Identifier: MIT
// NexKey - small Qt helpers.

#pragma once

#include <QByteArray>
#include <QString>

#include <string_view>

namespace nexkey::gui {

/// Convert a UTF-8 string view that comes from the core library.
[[nodiscard]] inline QString to_qstring(std::string_view text)
{
    return QString::fromUtf8(QByteArray(text.data(), static_cast<int>(text.size())));
}

} // namespace nexkey::gui
