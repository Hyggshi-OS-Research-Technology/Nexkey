// SPDX-License-Identifier: MIT
// NexKey - tray icon and menu.

#pragma once

#include <QIcon>
#include <QObject>
#include <QSystemTrayIcon>

class Daemon;
class QAction;
class QMenu;
class SettingsDialog;

class TrayApplication : public QObject {
    Q_OBJECT

public:
    explicit TrayApplication(Daemon* daemon, QObject* parent = nullptr);
    ~TrayApplication() override;

public slots:
    void showSettings();

private:
    void buildMenu();
    void updateStatus();
    void toggleMode();
    [[nodiscard]] static QIcon statusIcon(const QString& label, bool vietnamese);

    Daemon* m_daemon{};
    QSystemTrayIcon m_tray;
    QMenu* m_menu{};
    QAction* m_modeAction{};
    QAction* m_settingsAction{};
    SettingsDialog* m_dialog{};
};
