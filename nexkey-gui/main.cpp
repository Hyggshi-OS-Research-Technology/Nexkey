#include "DBusService.hpp"
#include "TrayManager.hpp"

#include <QApplication>
#include <QDBusConnection>

int main(int argc, char** argv)
{
    Q_INIT_RESOURCE(nexkey);

    QApplication app(argc, argv);
    app.setOrganizationName("nexkey");
    app.setApplicationName("nexkey");

    DBusService service;
    auto bus = QDBusConnection::sessionBus();
    bus.registerService("org.nexkey.Settings");
    bus.registerObject("/org/nexkey/Settings", &service, QDBusConnection::ExportAllSlots | QDBusConnection::ExportAllSignals);

    TrayManager tray(&service);
    return app.exec();
}
