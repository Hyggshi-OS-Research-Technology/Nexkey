// SPDX-License-Identifier: MIT
// NexKey - tray icon and menu.

#include "TrayApplication.hpp"

#include "Daemon.hpp"
#include "SettingsDialog.hpp"

#include <QAction>
#include <QApplication>
#include <QColor>
#include <QFont>
#include <QMenu>
#include <QPainter>
#include <QPixmap>

TrayApplication::TrayApplication(Daemon* daemon, QObject* parent)
    : QObject(parent)
    , m_daemon(daemon)
{
    buildMenu();

    m_tray.setContextMenu(m_menu);
    connect(&m_tray, &QSystemTrayIcon::activated, this, [this](QSystemTrayIcon::ActivationReason reason) {
        if (reason == QSystemTrayIcon::Trigger || reason == QSystemTrayIcon::DoubleClick) {
            showSettings();
        }
    });
    connect(m_daemon, &Daemon::modeChanged, this, [this] { updateStatus(); });
    connect(m_daemon, &Daemon::settingsChanged, this, [this] { updateStatus(); });
    connect(m_daemon, &Daemon::settingsRequested, this, &TrayApplication::showSettings);

    updateStatus();
}

TrayApplication::~TrayApplication()
{
    delete m_menu;
}

void TrayApplication::buildMenu()
{
    m_menu = new QMenu;

    m_modeAction = m_menu->addAction(tr("Vietnamese mode"));
    m_modeAction->setCheckable(true);
    connect(m_modeAction, &QAction::triggered, this, &TrayApplication::toggleMode);

    m_menu->addSeparator();

    m_settingsAction = m_menu->addAction(tr("Settings..."));
    connect(m_settingsAction, &QAction::triggered, this, &TrayApplication::showSettings);

    m_menu->addSeparator();

    auto* quit = m_menu->addAction(tr("Quit"));
    connect(quit, &QAction::triggered, qApp, &QApplication::quit);
}

QIcon TrayApplication::statusIcon(const QString& label, bool vietnamese)
{
    const int size = 64;
    QPixmap pixmap(size, size);
    pixmap.fill(Qt::transparent);

    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setPen(Qt::NoPen);
    painter.setBrush(vietnamese ? QColor(0x0F, 0x8B, 0x5F) : QColor(0x3B, 0x42, 0x52));
    painter.drawRoundedRect(QRectF(0, 0, size, size), 12, 12);

    QFont font = painter.font();
    font.setBold(true);
    font.setPixelSize(26);
    painter.setFont(font);
    painter.setPen(Qt::white);
    painter.drawText(QRectF(0, 0, size, size), Qt::AlignCenter, label);
    painter.end();

    return QIcon(pixmap);
}

void TrayApplication::updateStatus()
{
    const bool vietnamese = m_daemon->enabled();

    m_modeAction->setChecked(vietnamese);
    m_modeAction->setText(vietnamese ? tr("Vietnamese mode") : tr("English mode"));

    m_tray.setIcon(statusIcon(vietnamese ? QStringLiteral("VI") : QStringLiteral("EN"),
                              vietnamese));
    m_tray.setToolTip(tr("NexKey - %1").arg(vietnamese ? tr("Vietnamese") : tr("English")));
    m_tray.setVisible(m_daemon->settings().show_tray_icon);
}

void TrayApplication::toggleMode()
{
    m_daemon->Toggle();
}

void TrayApplication::showSettings()
{
    if (m_dialog == nullptr) {
        m_dialog = new SettingsDialog(m_daemon->settings());
        m_dialog->setAttribute(Qt::WA_DeleteOnClose);

        connect(m_dialog, &SettingsDialog::saved, this, [this](const nexkey::Settings& settings) {
            m_daemon->setSettings(settings);
            updateStatus();
        });
        connect(m_dialog, &QDialog::destroyed, this, [this] { m_dialog = nullptr; });
    }
    m_dialog->show();
    m_dialog->raise();
    m_dialog->activateWindow();
}
