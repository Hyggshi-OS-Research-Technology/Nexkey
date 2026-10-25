// SPDX-License-Identifier: MIT
// NexKey - tray icon and menu.

#include "TrayApplication.hpp"

#include "Daemon.hpp"
#include "SettingsDialog.hpp"

#include "EmojiPicker.hpp"

#include <QAction>
#include <QActionGroup>
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

void TrayApplication::updateOption(std::function<void(nexkey::Settings&)> mutator)
{
    auto s = m_daemon->settings();
    mutator(s);
    m_daemon->setSettings(s, true);
    updateStatus();
}

void TrayApplication::buildMenu()
{
    m_menu = new QMenu;

    // 1. Vietnamese Mode Toggle
    m_modeAction = m_menu->addAction(tr("Chế độ tiếng Việt (Vietnamese)"));
    m_modeAction->setCheckable(true);
    connect(m_modeAction, &QAction::triggered, this, &TrayApplication::toggleMode);

    m_menu->addSeparator();

    // 2. Input Method Submenu
    m_methodMenu = m_menu->addMenu(tr("Kiểu gõ (Input Method)"));
    m_methodGroup = new QActionGroup(this);
    m_methodGroup->setExclusive(true);

    const std::vector<std::pair<QString, nexkey::Method>> methods = {
        {tr("Telex"), nexkey::Method::Telex},
        {tr("Simple Telex"), nexkey::Method::SimpleTelex},
        {tr("VNI"), nexkey::Method::Vni},
        {tr("VIQR"), nexkey::Method::Viqr},
        {tr("Microsoft layout"), nexkey::Method::Microsoft}
    };
    for (const auto& [name, method] : methods) {
        auto* act = m_methodMenu->addAction(name);
        act->setCheckable(true);
        act->setData(static_cast<int>(method));
        m_methodGroup->addAction(act);
        connect(act, &QAction::triggered, this, [this, method] {
            updateOption([method](nexkey::Settings& s) { s.options.method = method; });
        });
    }

    // 3. Output Charset Submenu
    m_charsetMenu = m_menu->addMenu(tr("Bảng mã (Output Charset)"));
    m_charsetGroup = new QActionGroup(this);
    m_charsetGroup->setExclusive(true);

    const std::vector<std::pair<QString, nexkey::Encoding>> encodings = {
        {tr("Unicode (Mặc định)"), nexkey::Encoding::Unicode},
        {tr("Unicode tổ hợp (NFD)"), nexkey::Encoding::UnicodeDecomposed},
        {tr("TCVN3 (ABC)"), nexkey::Encoding::Tcvn3},
        {tr("VIQR"), nexkey::Encoding::Viqr}
    };
    for (const auto& [name, enc] : encodings) {
        auto* act = m_charsetMenu->addAction(name);
        act->setCheckable(true);
        act->setData(static_cast<int>(enc));
        m_charsetGroup->addAction(act);
        connect(act, &QAction::triggered, this, [this, enc] {
            updateOption([enc](nexkey::Settings& s) { s.options.encoding = enc; });
        });
    }

    m_menu->addSeparator();

    // 4. Quick Settings toggles from prompt
    m_spellCheckAction = m_menu->addAction(tr("Spell Check (Kiểm tra chính tả)"));
    m_spellCheckAction->setCheckable(true);
    connect(m_spellCheckAction, &QAction::triggered, this, [this](bool checked) {
        updateOption([checked](nexkey::Settings& s) { s.options.spell_check = checked; });
    });

    m_macroAction = m_menu->addAction(tr("Macro (Bật/tắt gõ tắt)"));
    m_macroAction->setCheckable(true);
    connect(m_macroAction, &QAction::triggered, this, [this](bool checked) {
        updateOption([checked](nexkey::Settings& s) { s.options.macro_enabled = checked; });
    });

    m_capMacroAction = m_menu->addAction(tr("Capitalize Macro (Tự viết hoa từ gõ tắt)"));
    m_capMacroAction->setCheckable(true);
    connect(m_capMacroAction, &QAction::triggered, this, [this](bool checked) {
        updateOption([checked](nexkey::Settings& s) { s.options.capitalize_macro = checked; });
    });

    m_autoRestoreAction = m_menu->addAction(tr("Auto Non-VN Restore (Tự khôi phục từ ngoại lai)"));
    m_autoRestoreAction->setCheckable(true);
    connect(m_autoRestoreAction, &QAction::triggered, this, [this](bool checked) {
        updateOption([checked](nexkey::Settings& s) { s.options.english_fallback = checked; });
    });

    m_customDictAction = m_menu->addAction(tr("Custom Dictionary (Từ điển cá nhân)"));
    m_customDictAction->setCheckable(true);
    connect(m_customDictAction, &QAction::triggered, this, [this](bool checked) {
        updateOption([checked](nexkey::Settings& s) { s.options.custom_dict = checked; });
    });

    m_menu->addSeparator();

    m_directTypingAction = m_menu->addAction(tr("Direct Typing (Gõ trực tiếp không gạch chân)"));
    m_directTypingAction->setCheckable(true);
    connect(m_directTypingAction, &QAction::triggered, this, [this](bool checked) {
        updateOption([checked](nexkey::Settings& s) { s.options.direct_typing = checked; });
    });

    m_emojiAction = m_menu->addAction(tr("Emoji Picker thông minh..."));
    connect(m_emojiAction, &QAction::triggered, this, &TrayApplication::showEmojiPicker);

    m_menu->addSeparator();

    m_settingsAction = m_menu->addAction(tr("Cài đặt (Settings)..."));
    connect(m_settingsAction, &QAction::triggered, this, &TrayApplication::showSettings);

    m_menu->addSeparator();

    auto* quit = m_menu->addAction(tr("Thoát (Quit)"));
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
    const auto& s = m_daemon->settings();
    const bool vietnamese = m_daemon->enabled();

    m_modeAction->setChecked(vietnamese);
    m_modeAction->setText(vietnamese ? tr("Chế độ tiếng Việt (VI)") : tr("Chế độ tiếng Anh (EN)"));

    for (auto* act : m_methodGroup->actions()) {
        act->setChecked(act->data().toInt() == static_cast<int>(s.options.method));
    }
    for (auto* act : m_charsetGroup->actions()) {
        act->setChecked(act->data().toInt() == static_cast<int>(s.options.encoding));
    }

    m_spellCheckAction->setChecked(s.options.spell_check);
    m_macroAction->setChecked(s.options.macro_enabled);
    m_capMacroAction->setChecked(s.options.capitalize_macro);
    m_autoRestoreAction->setChecked(s.options.english_fallback);
    m_customDictAction->setChecked(s.options.custom_dict);
    m_directTypingAction->setChecked(s.options.direct_typing);

    m_tray.setIcon(statusIcon(vietnamese ? QStringLiteral("VI") : QStringLiteral("EN"),
                              vietnamese));
    m_tray.setToolTip(tr("NexKey - %1 (%2)").arg(
        vietnamese ? tr("Tiếng Việt") : tr("English"),
        QString::fromStdString(std::string(nexkey::to_string(s.options.method)))));
    m_tray.setVisible(s.show_tray_icon);
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

void TrayApplication::showEmojiPicker()
{
    if (m_emojiPicker == nullptr) {
        m_emojiPicker = new EmojiPicker();
        m_emojiPicker->setAttribute(Qt::WA_DeleteOnClose);
        connect(m_emojiPicker, &QDialog::destroyed, this, [this] { m_emojiPicker = nullptr; });
    }
    m_emojiPicker->show();
    m_emojiPicker->raise();
    m_emojiPicker->activateWindow();
}
