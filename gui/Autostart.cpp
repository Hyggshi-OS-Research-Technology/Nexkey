// SPDX-License-Identifier: MIT
// NexKey - autostart entry handling.

#include "Autostart.hpp"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>
#include <QString>

namespace nexkey::gui {

namespace {

constexpr auto kSystemEntry = "/etc/xdg/autostart/nexkey.desktop";

QString user_entry_path()
{
    return QStandardPaths::writableLocation(QStandardPaths::ConfigLocation) +
           QStringLiteral("/autostart/nexkey.desktop");
}

bool write_entry(const QString& path, bool hidden)
{
    QDir().mkpath(QFileInfo(path).absolutePath());

    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
        return false;
    }

    const QByteArray contents =
        QByteArray("[Desktop Entry]\n"
                   "Type=Application\n"
                   "Name=NexKey\n"
                   "GenericName=Vietnamese Input Method\n"
                   "Comment=Start the NexKey tray icon with the desktop session\n"
                   "Exec=nexkey-gui\n"
                   "Icon=nexkey\n"
                   "Terminal=false\n"
                   "NoDisplay=true\n") +
        (hidden ? QByteArray("Hidden=true\n") : QByteArray("Hidden=false\n")) +
        QByteArray("X-GNOME-Autostart-enabled=true\n");

    return file.write(contents) == contents.size();
}

} // namespace

void apply_autostart(bool enabled)
{
    const auto user_path = user_entry_path();

    if (enabled) {
        // A packaged system entry already starts the tray, so the override is
        // removed; without one, a per user entry is written.
        QFile::remove(user_path);
        if (!QFileInfo::exists(QString::fromLatin1(kSystemEntry))) {
            static_cast<void>(write_entry(user_path, false));
        }
        return;
    }

    static_cast<void>(write_entry(user_path, true));
}

bool autostart_installed()
{
    const auto user_path = user_entry_path();
    QFile user(user_path);
    if (user.exists()) {
        if (!user.open(QIODevice::ReadOnly | QIODevice::Text)) {
            return false;
        }
        return !QString::fromUtf8(user.readAll()).contains(QStringLiteral("Hidden=true"),
                                                          Qt::CaseInsensitive);
    }
    return QFileInfo::exists(QString::fromLatin1(kSystemEntry));
}

} // namespace nexkey::gui
