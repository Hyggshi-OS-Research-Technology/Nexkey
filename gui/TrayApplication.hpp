// SPDX-License-Identifier: MIT
// NexKey - tray icon and menu.

#pragma once

#include <QIcon>
#include <QObject>
#include <QSystemTrayIcon>

#include "nexkey/Settings.hpp"

#include <functional>

class Daemon;
class QAction;
class QMenu;
class SettingsDialog;

class EmojiPicker;
class QActionGroup;

class TrayApplication : public QObject {
    Q_OBJECT

public:
    explicit TrayApplication(Daemon* daemon, QObject* parent = nullptr);
    ~TrayApplication() override;

public slots:
    void showSettings();
    void showEmojiPicker();

private:
    void buildMenu();
    void updateStatus();
    void toggleMode();
    void updateOption(std::function<void(nexkey::Settings&)> mutator);
    [[nodiscard]] static QIcon statusIcon(const QString& label, bool vietnamese);

    Daemon* m_daemon{};
    QSystemTrayIcon m_tray;
    QMenu* m_menu{};
    QAction* m_modeAction{};
    QMenu* m_methodMenu{};
    QActionGroup* m_methodGroup{};
    QMenu* m_charsetMenu{};
    QActionGroup* m_charsetGroup{};

    QAction* m_spellCheckAction{};
    QAction* m_macroAction{};
    QAction* m_capMacroAction{};
    QAction* m_autoRestoreAction{};
    QAction* m_customDictAction{};
    QAction* m_directTypingAction{};
    QAction* m_emojiAction{};

    QAction* m_settingsAction{};
    SettingsDialog* m_dialog{};
    EmojiPicker* m_emojiPicker{};
};
