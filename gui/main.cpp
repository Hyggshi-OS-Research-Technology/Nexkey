// SPDX-License-Identifier: MIT
// NexKey tray application entry point.

#include "Daemon.hpp"
#include "TrayApplication.hpp"

#include "QtHelpers.hpp"
#include "nexkey/Types.hpp"

#include <QApplication>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QString>
#include <QSystemTrayIcon>

namespace {

constexpr auto kServiceName = "org.nexkey.Daemon";
constexpr auto kObjectPath = "/org/nexkey/Daemon";

/// Ask the already running instance to show its settings window.
void activate_running_instance()
{
    const auto message = QDBusMessage::createMethodCall(QStringLiteral(kServiceName),
                                                        QStringLiteral(kObjectPath),
                                                        QStringLiteral(kServiceName),
                                                        QStringLiteral("ShowSettings"));
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
    QApplication::setQuitOnLastWindowClosed(false);

    bool open_settings = !QSystemTrayIcon::isSystemTrayAvailable();
    const auto arguments = QApplication::arguments();
    for (const auto& argument : arguments) {
        if (argument == "--settings" || argument == "-s") {
            open_settings = true;
        }
    }

    auto bus = QDBusConnection::sessionBus();
    const bool bus_ready = bus.isConnected();
    if (bus_ready && !bus.registerService(QString::fromLatin1(kServiceName))) {
        activate_running_instance();
        return 0;
    }

    Daemon daemon;
    if (bus_ready) {
        bus.registerObject(QString::fromLatin1(kObjectPath), &daemon,
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
