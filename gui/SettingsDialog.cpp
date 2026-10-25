// SPDX-License-Identifier: MIT
// NexKey - settings dialog.

#include "SettingsDialog.hpp"

#include "Autostart.hpp"
#include "QtHelpers.hpp"

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QIcon>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QTabWidget>
#include <QTableWidget>
#include <QVBoxLayout>

#include <utility>
#include <vector>

namespace {

/// Fill a combo box from a list of (label, value) pairs.
void fill_combo(QComboBox* combo, const std::vector<std::pair<QString, int>>& entries)
{
    for (const auto& [label, value] : entries) {
        combo->addItem(label, value);
    }
}

} // namespace

SettingsDialog::SettingsDialog(const nexkey::Settings& settings, QWidget* parent)
    : QDialog(parent)
    , m_settings(settings)
{
    setWindowTitle(tr("NexKey Settings"));
    setWindowIcon(QIcon(QStringLiteral(":/icons/nexkey.svg")));
    resize(520, 480);

    // ==========================================
    // 1. INPUT TAB (Kiểu gõ & Presets)
    // ==========================================
    m_method = new QComboBox(this);
    fill_combo(m_method, {{tr("Telex"), static_cast<int>(nexkey::Method::Telex)},
                          {tr("Simple Telex"), static_cast<int>(nexkey::Method::SimpleTelex)},
                          {tr("VNI"), static_cast<int>(nexkey::Method::Vni)},
                          {tr("VIQR"), static_cast<int>(nexkey::Method::Viqr)},
                          {tr("Microsoft layout"), static_cast<int>(nexkey::Method::Microsoft)}});

    m_toneStyle = new QComboBox(this);
    fill_combo(m_toneStyle,
               {{tr("Modern - oà/uý (hoa with tone on a)"), static_cast<int>(nexkey::ToneStyle::Modern)},
                {tr("Traditional - òa/úy (hoa with tone on o)"),
                 static_cast<int>(nexkey::ToneStyle::Traditional)}});

    m_encoding = new QComboBox(this);
    fill_combo(m_encoding, {{tr("Unicode (Mặc định)"), static_cast<int>(nexkey::Encoding::Unicode)},
                            {tr("Unicode decomposed (NFD)"),
                             static_cast<int>(nexkey::Encoding::UnicodeDecomposed)},
                            {tr("TCVN3 (ABC)"), static_cast<int>(nexkey::Encoding::Tcvn3)},
                            {tr("VIQR"), static_cast<int>(nexkey::Encoding::Viqr)}});

    auto* inputPage = new QWidget(this);
    auto* inputLayout = new QVBoxLayout(inputPage);

    auto* inputForm = new QFormLayout();
    inputForm->addRow(tr("Input method (Kiểu gõ)"), m_method);
    inputForm->addRow(tr("Tone placement (Đặt dấu)"), m_toneStyle);
    inputForm->addRow(tr("Output charset (Bảng mã)"), m_encoding);
    inputLayout->addLayout(inputForm);

    auto* presetGroup = new QGroupBox(tr("Preset bàn phím (Keyboard Presets)"), inputPage);
    auto* presetLayout = new QHBoxLayout(presetGroup);

    auto* telexBtn = new QPushButton(tr("Telex"), presetGroup);
    auto* vniBtn = new QPushButton(tr("VNI"), presetGroup);
    auto* viqrBtn = new QPushButton(tr("VIQR"), presetGroup);
    auto* msBtn = new QPushButton(tr("Microsoft"), presetGroup);

    connect(telexBtn, &QPushButton::clicked, this, [this] { applyPreset(nexkey::Method::Telex); });
    connect(vniBtn, &QPushButton::clicked, this, [this] { applyPreset(nexkey::Method::Vni); });
    connect(viqrBtn, &QPushButton::clicked, this, [this] { applyPreset(nexkey::Method::Viqr); });
    connect(msBtn, &QPushButton::clicked, this, [this] { applyPreset(nexkey::Method::Microsoft); });

    presetLayout->addWidget(telexBtn);
    presetLayout->addWidget(vniBtn);
    presetLayout->addWidget(viqrBtn);
    presetLayout->addWidget(msBtn);
    inputLayout->addWidget(presetGroup);
    inputLayout->addStretch();

    // ==========================================
    // 2. ENGINE TAB (Bộ gõ & Tính năng)
    // ==========================================
    m_directTyping = new QCheckBox(tr("Direct typing (Gõ trực tiếp, không gạch chân - Non-preedit)"), this);
    m_englishFallback = new QCheckBox(tr("Auto Non-VN Restore (Tự động khôi phục khi gõ từ ngoại lai)"), this);
    m_spellCheck = new QCheckBox(tr("Spell Check (Kiểm tra lỗi chính tả bằng từ điển)"), this);
    m_macroEnabled = new QCheckBox(tr("Macro (Bật/tắt gõ tắt)"), this);
    m_capitalizeMacro = new QCheckBox(tr("Capitalize Macro (Tự động viết hoa từ gõ tắt)"), this);
    m_customDict = new QCheckBox(tr("Custom Dictionary (Sử dụng từ điển cá nhân)"), this);
    m_smartBackspace = new QCheckBox(tr("Smart backspace (Xóa thông minh theo âm tiết)"), this);
    m_bracketShortcuts = new QCheckBox(tr("Telex bracket shortcuts ([ { cho ơ ư)"), this);
    m_candidates = new QCheckBox(tr("Show the word selection window (Cửa sổ gợi ý từ)"), this);

    m_timeFormat = new QLineEdit(this);
    m_timeFormat->setPlaceholderText(QStringLiteral("%H:%M:%S"));
    m_dateFormat = new QLineEdit(this);
    m_dateFormat->setPlaceholderText(QStringLiteral("%d/%m/%Y"));

    auto* enginePage = new QWidget(this);
    auto* engineLayout = new QVBoxLayout(enginePage);
    engineLayout->addWidget(m_directTyping);
    engineLayout->addWidget(m_englishFallback);
    engineLayout->addWidget(m_spellCheck);
    engineLayout->addWidget(m_macroEnabled);
    engineLayout->addWidget(m_capitalizeMacro);
    engineLayout->addWidget(m_customDict);
    engineLayout->addWidget(m_smartBackspace);
    engineLayout->addWidget(m_bracketShortcuts);
    engineLayout->addWidget(m_candidates);

    auto* macroFmtGroup = new QGroupBox(tr("Gõ tắt $TIME / $DATE Format"), enginePage);
    auto* macroFmtLayout = new QFormLayout(macroFmtGroup);
    macroFmtLayout->addRow(tr("Time Format"), m_timeFormat);
    macroFmtLayout->addRow(tr("Date Format"), m_dateFormat);
    engineLayout->addWidget(macroFmtGroup);
    engineLayout->addStretch();

    // ==========================================
    // 3. APP RULES TAB (Chế độ theo ứng dụng)
    // ==========================================
    auto* appPage = new QWidget(this);
    auto* appLayout = new QVBoxLayout(appPage);
    auto* appDesc = new QLabel(tr("Gán chế độ gõ riêng cho từng ứng dụng hoặc từng cửa sổ:"), appPage);
    appDesc->setWordWrap(true);
    appLayout->addWidget(appDesc);

    m_appRulesTable = new QTableWidget(0, 2, appPage);
    m_appRulesTable->setHorizontalHeaderLabels({tr("Ứng dụng (App/Process)"), tr("Chế độ (Mode)")});
    m_appRulesTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_appRulesTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    appLayout->addWidget(m_appRulesTable);

    auto* appBtnLayout = new QHBoxLayout();
    m_addRuleBtn = new QPushButton(tr("Thêm quy tắc..."), appPage);
    m_removeRuleBtn = new QPushButton(tr("Xóa quy tắc"), appPage);
    connect(m_addRuleBtn, &QPushButton::clicked, this, &SettingsDialog::addAppRule);
    connect(m_removeRuleBtn, &QPushButton::clicked, this, &SettingsDialog::removeAppRule);
    appBtnLayout->addWidget(m_addRuleBtn);
    appBtnLayout->addWidget(m_removeRuleBtn);
    appBtnLayout->addStretch();
    appLayout->addLayout(appBtnLayout);

    // ==========================================
    // 4. BACKUP & RESTORE TAB (Sao lưu & Phục hồi)
    // ==========================================
    auto* backupPage = new QWidget(this);
    auto* backupLayout = new QVBoxLayout(backupPage);

    auto* bakDesc = new QLabel(tr("Xuất/nhập toàn bộ cấu hình dạng file JSON, chọn lọc từng thành phần:"), backupPage);
    bakDesc->setWordWrap(true);
    backupLayout->addWidget(bakDesc);

    auto* bakGroup = new QGroupBox(tr("Thành phần sao lưu"), backupPage);
    auto* bakGroupLayout = new QVBoxLayout(bakGroup);
    m_bakMainSettings = new QCheckBox(tr("Main Settings (Cấu hình chung)"), bakGroup);
    m_bakMacros = new QCheckBox(tr("Macros (Gõ tắt & Emoji)"), bakGroup);
    m_bakKeymaps = new QCheckBox(tr("Custom Keymaps (Preset bàn phím)"), bakGroup);
    m_bakAppRules = new QCheckBox(tr("App Rules (Quy tắc ứng dụng)"), bakGroup);
    m_bakCustomDict = new QCheckBox(tr("Custom Dictionary (Từ điển cá nhân)"), bakGroup);

    m_bakMainSettings->setChecked(true);
    m_bakMacros->setChecked(true);
    m_bakKeymaps->setChecked(true);
    m_bakAppRules->setChecked(true);
    m_bakCustomDict->setChecked(true);

    bakGroupLayout->addWidget(m_bakMainSettings);
    bakGroupLayout->addWidget(m_bakMacros);
    bakGroupLayout->addWidget(m_bakKeymaps);
    bakGroupLayout->addWidget(m_bakAppRules);
    bakGroupLayout->addWidget(m_bakCustomDict);
    backupLayout->addWidget(bakGroup);

    auto* bakActionLayout = new QHBoxLayout();
    m_exportBtn = new QPushButton(tr("Xuất cấu hình JSON (Export)..."), backupPage);
    m_importBtn = new QPushButton(tr("Nhập cấu hình JSON (Import)..."), backupPage);
    connect(m_exportBtn, &QPushButton::clicked, this, &SettingsDialog::exportJson);
    connect(m_importBtn, &QPushButton::clicked, this, &SettingsDialog::importJson);
    bakActionLayout->addWidget(m_exportBtn);
    bakActionLayout->addWidget(m_importBtn);
    backupLayout->addLayout(bakActionLayout);
    backupLayout->addStretch();

    // ==========================================
    // 5. INTERFACE TAB (Giao diện)
    // ==========================================
    m_toggleKey = new QLineEdit(this);
    m_toggleKey->setPlaceholderText(QStringLiteral("Control+Shift"));
    m_showTrayIcon = new QCheckBox(tr("Show the tray icon (Hiện biểu tượng khay hệ thống)"), this);
    m_startWithSystem = new QCheckBox(tr("Start automatically when I sign in (Khởi động cùng hệ thống)"), this);

    auto* interfacePage = new QWidget(this);
    auto* interfaceLayout = new QFormLayout(interfacePage);
    interfaceLayout->addRow(tr("Toggle key (Phím chuyển VI/EN)"), m_toggleKey);
    interfaceLayout->addRow(QString{}, m_showTrayIcon);
    interfaceLayout->addRow(QString{}, m_startWithSystem);

    // ==========================================
    // 6. ABOUT TAB (Thông tin)
    // ==========================================
    auto* aboutPage = new QWidget(this);
    auto* aboutLayout = new QVBoxLayout(aboutPage);
    auto* about = new QLabel(tr("NexKey %1\n"
                               "Bộ gõ tiếng Việt thông minh cho Linux.\n"
                               "Gõ trực tiếp (Non-preedit, không gạch chân).\n"
                               "Hỗ trợ Uinput, Wayland & X11, App Rules, Macros, Smart Emoji.\n\n"
                               "Đường dẫn file cấu hình:\n%2")
                                 .arg(nexkey::gui::to_qstring(nexkey::kVersion),
                                      QString::fromStdString(nexkey::default_config_path().string())),
                            aboutPage);
    about->setWordWrap(true);
    aboutLayout->addWidget(about);
    aboutLayout->addStretch();

    auto* tabs = new QTabWidget(this);
    tabs->addTab(inputPage, tr("Kiểu gõ"));
    tabs->addTab(enginePage, tr("Tính năng"));
    tabs->addTab(appPage, tr("Ứng dụng"));
    tabs->addTab(backupPage, tr("Sao lưu & Phục hồi"));
    tabs->addTab(interfacePage, tr("Giao diện"));
    tabs->addTab(aboutPage, tr("Thông tin"));

    auto* buttons = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel | QDialogButtonBox::Apply, this);

    auto* layout = new QVBoxLayout(this);
    layout->addWidget(tabs);
    layout->addWidget(buttons);

    connect(buttons, &QDialogButtonBox::accepted, this, [this] {
        apply();
        accept();
    });
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(buttons->button(QDialogButtonBox::Apply), &QPushButton::clicked, this,
            [this] { apply(); });

    load(m_settings);
}

void SettingsDialog::applyPreset(nexkey::Method method)
{
    m_method->setCurrentIndex(m_method->findData(static_cast<int>(method)));
    if (method == nexkey::Method::Telex || method == nexkey::Method::Microsoft) {
        m_bracketShortcuts->setChecked(true);
        m_toneStyle->setCurrentIndex(m_toneStyle->findData(static_cast<int>(nexkey::ToneStyle::Modern)));
    } else if (method == nexkey::Method::Vni) {
        m_bracketShortcuts->setChecked(false);
    }
}

void SettingsDialog::load(const nexkey::Settings& settings)
{
    m_settings = settings;
    m_method->setCurrentIndex(m_method->findData(static_cast<int>(settings.options.method)));
    m_toneStyle->setCurrentIndex(
        m_toneStyle->findData(static_cast<int>(settings.options.tone_style)));
    m_encoding->setCurrentIndex(m_encoding->findData(static_cast<int>(settings.options.encoding)));
    m_directTyping->setChecked(settings.options.direct_typing);
    m_smartBackspace->setChecked(settings.options.smart_backspace);
    m_englishFallback->setChecked(settings.options.english_fallback);
    m_spellCheck->setChecked(settings.options.spell_check);
    m_macroEnabled->setChecked(settings.options.macro_enabled);
    m_capitalizeMacro->setChecked(settings.options.capitalize_macro);
    m_customDict->setChecked(settings.options.custom_dict);
    m_bracketShortcuts->setChecked(settings.options.bracket_shortcuts);
    m_candidates->setChecked(settings.options.candidates);

    m_timeFormat->setText(QString::fromStdString(settings.time_format));
    m_dateFormat->setText(QString::fromStdString(settings.date_format));

    m_showTrayIcon->setChecked(settings.show_tray_icon);
    m_startWithSystem->setChecked(settings.start_with_system);
    m_toggleKey->setText(QString::fromStdString(settings.toggle_key));

    // Load App Rules
    m_appRulesTable->setRowCount(0);
    int r = 0;
    for (const auto& [app, mode] : settings.app_rules) {
        m_appRulesTable->insertRow(r);
        m_appRulesTable->setItem(r, 0, new QTableWidgetItem(QString::fromStdString(app)));
        m_appRulesTable->setItem(r, 1, new QTableWidgetItem(QString::fromStdString(mode)));
        ++r;
    }
}

nexkey::Settings SettingsDialog::settings() const
{
    nexkey::Settings result = m_settings;
    result.options.method = static_cast<nexkey::Method>(m_method->currentData().toInt());
    result.options.tone_style = static_cast<nexkey::ToneStyle>(m_toneStyle->currentData().toInt());
    result.options.encoding = static_cast<nexkey::Encoding>(m_encoding->currentData().toInt());
    result.options.direct_typing = m_directTyping->isChecked();
    result.options.smart_backspace = m_smartBackspace->isChecked();
    result.options.english_fallback = m_englishFallback->isChecked();
    result.options.spell_check = m_spellCheck->isChecked();
    result.options.macro_enabled = m_macroEnabled->isChecked();
    result.options.capitalize_macro = m_capitalizeMacro->isChecked();
    result.options.custom_dict = m_customDict->isChecked();
    result.options.bracket_shortcuts = m_bracketShortcuts->isChecked();
    result.options.candidates = m_candidates->isChecked();

    result.time_format = m_timeFormat->text().trimmed().toStdString();
    if (result.time_format.empty()) result.time_format = "%H:%M:%S";
    result.date_format = m_dateFormat->text().trimmed().toStdString();
    if (result.date_format.empty()) result.date_format = "%d/%m/%Y";

    result.show_tray_icon = m_showTrayIcon->isChecked();
    result.start_with_system = m_startWithSystem->isChecked();
    result.toggle_key = m_toggleKey->text().toStdString();

    // App Rules
    result.app_rules.clear();
    for (int i = 0; i < m_appRulesTable->rowCount(); ++i) {
        const auto* appItem = m_appRulesTable->item(i, 0);
        const auto* modeItem = m_appRulesTable->item(i, 1);
        if (appItem != nullptr && modeItem != nullptr) {
            const auto app = appItem->text().trimmed().toStdString();
            const auto mode = modeItem->text().trimmed().toStdString();
            if (!app.empty() && !mode.empty()) {
                result.app_rules[app] = mode;
            }
        }
    }

    return result;
}

void SettingsDialog::apply()
{
    m_settings = settings();
    if (!nexkey::save_settings(m_settings)) {
        QMessageBox::warning(this, tr("NexKey"),
                             tr("The configuration file could not be written:\n%1")
                                 .arg(QString::fromStdString(nexkey::default_config_path().string())));
        return;
    }
    nexkey::gui::apply_autostart(m_settings.start_with_system);
    emit saved(m_settings);
}

void SettingsDialog::addAppRule()
{
    bool ok = false;
    const QString app = QInputDialog::getText(
        this, tr("Thêm quy tắc ứng dụng"),
        tr("Tên ứng dụng/tiến trình (ví dụ: code, gnome-terminal, firefox):"),
        QLineEdit::Normal, QString{}, &ok);
    if (!ok || app.trimmed().isEmpty()) return;

    QStringList modes = {QStringLiteral("telex"), QStringLiteral("vni"),
                         QStringLiteral("viqr"), QStringLiteral("microsoft"),
                         QStringLiteral("off (English)")};
    const QString mode = QInputDialog::getItem(
        this, tr("Chọn chế độ"),
        tr("Chế độ gõ cho %1:").arg(app),
        modes, 0, false, &ok);
    if (!ok) return;

    const int row = m_appRulesTable->rowCount();
    m_appRulesTable->insertRow(row);
    m_appRulesTable->setItem(row, 0, new QTableWidgetItem(app.trimmed().toLower()));
    m_appRulesTable->setItem(row, 1, new QTableWidgetItem(mode.contains("off") ? QStringLiteral("off") : mode));
}

void SettingsDialog::removeAppRule()
{
    const int row = m_appRulesTable->currentRow();
    if (row >= 0) {
        m_appRulesTable->removeRow(row);
    }
}

void SettingsDialog::exportJson()
{
    const QString path = QFileDialog::getSaveFileName(
        this, tr("Xuất file JSON sao lưu"),
        QStringLiteral("nexkey_backup.json"),
        tr("JSON Files (*.json)"));
    if (path.isEmpty()) return;

    auto current = settings();
    // Filter by selective checkboxes
    nexkey::Settings filtered = current;
    if (!m_bakMacros->isChecked()) filtered.macros.clear();
    if (!m_bakAppRules->isChecked()) filtered.app_rules.clear();
    if (!m_bakCustomDict->isChecked()) filtered.custom_dictionary.clear();

    if (nexkey::export_backup_file(path.toStdString(), filtered)) {
        QMessageBox::information(this, tr("Sao lưu thành công"),
                                tr("Toàn bộ cấu hình đã được xuất ra file:\n%1").arg(path));
    } else {
        QMessageBox::critical(this, tr("Lỗi sao lưu"), tr("Không thể ghi file:\n%1").arg(path));
    }
}

void SettingsDialog::importJson()
{
    const QString path = QFileDialog::getOpenFileName(
        this, tr("Nhập file JSON cấu hình"),
        QString{},
        tr("JSON Files (*.json)"));
    if (path.isEmpty()) return;

    nexkey::Settings imported = m_settings;
    if (nexkey::import_backup_file(path.toStdString(), imported)) {
        // Selective restore
        auto current = settings();
        if (m_bakMainSettings->isChecked()) {
            current.options = imported.options;
            current.enabled = imported.enabled;
            current.toggle_key = imported.toggle_key;
            current.show_tray_icon = imported.show_tray_icon;
            current.start_with_system = imported.start_with_system;
            current.time_format = imported.time_format;
            current.date_format = imported.date_format;
        }
        if (m_bakMacros->isChecked() && !imported.macros.empty()) {
            current.macros = imported.macros;
            if (!nexkey::save_macros(current.macros)) {
                QMessageBox::warning(this, tr("NexKey"),
                                     tr("Không thể ghi file gõ tắt:\n%1")
                                         .arg(QString::fromStdString(nexkey::default_macros_path().string())));
            }
        }
        if (m_bakKeymaps->isChecked()) {
            current.keymap_preset = imported.keymap_preset;
        }
        if (m_bakAppRules->isChecked()) {
            current.app_rules = imported.app_rules;
        }
        if (m_bakCustomDict->isChecked()) {
            current.custom_dictionary = imported.custom_dictionary;
        }
        load(current);
        apply();
        QMessageBox::information(this, tr("Phục hồi thành công"),
                                tr("Đã nhập thành công cấu hình từ file:\n%1").arg(path));
    } else {
        QMessageBox::critical(this, tr("Lỗi phục hồi"), tr("Không thể đọc hoặc parse file JSON:\n%1").arg(path));
    }
}
