#include "TrayManager.hpp"

#include "DBusService.hpp"
#include "SettingsWindow.hpp"

#include <QAction>
#include <QApplication>
#include <QMenu>

TrayManager::TrayManager(DBusService* service, QObject* parent)
    : QObject(parent)
    , m_service(service)
{
    auto* menu = new QMenu;
    menu->addAction("Toggle VI/EN", this, [this] { toggle(); });
    menu->addAction("Open Settings", this, [this] { open_settings(); });
    menu->addAction("Restart Engine");
    menu->addSeparator();
    menu->addAction("Quit", qApp, &QApplication::quit);

    m_tray.setContextMenu(menu);
    m_tray.setIcon(icon_for_mode());
    m_tray.setToolTip("NexKey");
    connect(&m_tray, &QSystemTrayIcon::activated, this, [this](QSystemTrayIcon::ActivationReason reason) {
        if (reason == QSystemTrayIcon::Trigger) {
            toggle();
        }
    });
    connect(m_service, &DBusService::ModeChanged, this, [this] { m_tray.setIcon(icon_for_mode()); });
    m_tray.show();
}

void TrayManager::toggle()
{
    m_service->SetMode(m_service->GetMode() == "VI" ? "EN" : "VI");
}

void TrayManager::open_settings()
{
    if (m_settings == nullptr) {
        m_settings = new SettingsWindow;
    }
    m_settings->show();
    m_settings->raise();
    m_settings->activateWindow();
}

QIcon TrayManager::icon_for_mode() const
{
    return QIcon(m_service->GetMode() == "VI" ? ":/icons/nexkey-vi.svg" : ":/icons/nexkey-en.svg");
}
