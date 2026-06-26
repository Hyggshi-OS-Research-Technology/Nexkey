#pragma once

#include <QObject>
#include <QSystemTrayIcon>

class DBusService;
class SettingsWindow;

class TrayManager : public QObject {
    Q_OBJECT

public:
    explicit TrayManager(DBusService* service, QObject* parent = nullptr);

private:
    void toggle();
    void open_settings();
    QIcon icon_for_mode() const;

    DBusService* m_service{};
    QSystemTrayIcon m_tray;
    SettingsWindow* m_settings{};
};
