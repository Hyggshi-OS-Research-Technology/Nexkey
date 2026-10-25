// SPDX-License-Identifier: MIT
// NexKey tray application entry point.

#include "Daemon.hpp"
#include "TrayApplication.hpp"

#include "QtHelpers.hpp"
#include "nexkey/Types.hpp"

#include <QApplication>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QIcon>
#include <QString>
#include <QSystemTrayIcon>

namespace {

// Qt string literals have to stay literals at the point of use, so the bus
// names are spelled out where they are needed.
constexpr auto kShowSettingsMethod = "ShowSettings";

/// Ask the already running instance to show its settings window.
void activate_running_instance()
{
    const auto message =
        QDBusMessage::createMethodCall(QStringLiteral("org.nexkey.Daemon"),
                                       QStringLiteral("/org/nexkey/Daemon"),
                                       QStringLiteral("org.nexkey.Daemon"),
                                       QString::fromLatin1(kShowSettingsMethod));
    QDBusConnection::sessionBus().call(message, QDBus::NoBlock);
}

} // namespace

int main(int argc, char** argv)
{
    QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral("NexKey"));
    QApplication::setApplicationDisplayName(QStringLiteral("NexKey"));
    QApplication::setOrganizationName(QStringLiteral("NexKey"));
    QApplication::setApplicationVersion(nexkey::gui::to_qstring(nexkey::kVersion));
    QApplication::setWindowIcon(QIcon(QStringLiteral(":/icons/nexkey.svg")));
    QApplication::setQuitOnLastWindowClosed(false);

    bool open_settings = !QSystemTrayIcon::isSystemTrayAvailable();
    const auto arguments = QApplication::arguments();
    for (const auto& argument : arguments) {
        if (argument == QStringLiteral("--settings") || argument == QStringLiteral("-s")) {
            open_settings = true;
        }
    }

    auto bus = QDBusConnection::sessionBus();
    const bool bus_ready = bus.isConnected();
    if (bus_ready && !bus.registerService(QStringLiteral("org.nexkey.Daemon"))) {
        activate_running_instance();
        return 0;
    }

    Daemon daemon;
    if (bus_ready) {
        bus.registerObject(QStringLiteral("/org/nexkey/Daemon"), &daemon,
                           QDBusConnection::ExportAllSlots |
                               QDBusConnection::ExportAllSignals |
                               QDBusConnection::ExportAllProperties);
    }

    TrayApplication tray(&daemon);
    if (open_settings) {
        tray.showSettings();
    }

    return QApplication::exec();
}
