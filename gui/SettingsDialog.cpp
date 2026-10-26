// SPDX-License-Identifier: MIT
// NexKey - settings dialog.

#include "SettingsDialog.hpp"

#include "Autostart.hpp"
#include "Diagnostics.hpp"
#include "QtHelpers.hpp"

#include <QCheckBox>
#include <QComboBox>
#include <QClipboard>
#include <QDesktopServices>
#include <QFileDialog>
#include <QFormLayout>
#include <QFontDatabase>
#include <QFrame>
#include <QGuiApplication>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QIcon>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QSizePolicy>
#include <QStackedWidget>
#include <QStyle>
#include <QTimer>
#include <QTableWidget>
#include <QUrl>
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

/// Theme icon with a Qt standard icon as fallback.
QIcon themed_icon(const QStringList& names, const QStyle* style, QStyle::StandardPixmap fallback)
{
    for (const auto& name : names) {
        if (QIcon::hasThemeIcon(name)) {
            return QIcon::fromTheme(name);
        }
    }
    return style->standardIcon(fallback);
}

/// Put a page into a frameless scroll area so small windows stay usable.
QScrollArea* wrap_page(QWidget* page, QWidget* parent)
{
    auto* scroll = new QScrollArea(parent);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setWidgetResizable(true);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll->setWidget(page);
    return scroll;
}

void prepare_page_layout(QVBoxLayout* layout)
{
    layout->setContentsMargins(24, 20, 24, 20);
    layout->setSpacing(14);
}

const QString kRepoUrl = QStringLiteral("https://github.com/Hyggshi-OS-Research-Technology/Nexkey");
const QString kOrgUrl = QStringLiteral("https://github.com/Hyggshi-OS-Research-Technology");

const char* const kStyleSheet = R"(
QListWidget#sidebar {
    background: palette(base);
    border: 1px solid palette(mid);
    border-radius: 8px;
    padding: 6px;
    outline: 0;
}
QListWidget#sidebar::item {
    padding: 10px 8px;
    margin: 2px 0;
    border-radius: 10px;
}
QListWidget#sidebar::item:hover:!selected {
    background: palette(alternate-base);
}
QListWidget#sidebar::item:selected,
QListWidget#sidebar::item:selected:!active {
    background: #e95420;
    color: white;
}
QLabel#aboutTitle {
    font-size: 30px;
}
QLabel#versionBadge {
    background: #e95420;
    color: white;
    font-weight: 600;
    padding: 6px 16px;
    border-radius: 3px;
}
QPushButton#okButton {
    font-weight: 600;
}
)";

} // namespace

SettingsDialog::SettingsDialog(const nexkey::Settings& settings, QWidget* parent)
    : QDialog(parent)
    , m_settings(settings)
{
    setWindowTitle(tr("Cài đặt NexKey"));
    setWindowIcon(QIcon(QStringLiteral(":/icons/nexkey.svg")));
    resize(860, 640);
    setMinimumSize(720, 520);
    setStyleSheet(QString::fromUtf8(kStyleSheet));

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
    prepare_page_layout(inputLayout);

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
    prepare_page_layout(engineLayout);

    auto* typingGroup = new QGroupBox(tr("Gõ và chính tả"), enginePage);
    auto* typingLayout = new QVBoxLayout(typingGroup);
    typingLayout->addWidget(m_directTyping);
    typingLayout->addWidget(m_englishFallback);
    typingLayout->addWidget(m_spellCheck);
    typingLayout->addWidget(m_smartBackspace);
    typingLayout->addWidget(m_bracketShortcuts);
    typingLayout->addWidget(m_candidates);
    engineLayout->addWidget(typingGroup);

    auto* macroGroup = new QGroupBox(tr("Gõ tắt và từ điển"), enginePage);
    auto* macroLayout = new QVBoxLayout(macroGroup);
    macroLayout->addWidget(m_macroEnabled);
    macroLayout->addWidget(m_capitalizeMacro);
    macroLayout->addWidget(m_customDict);
    engineLayout->addWidget(macroGroup);

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
    prepare_page_layout(appLayout);
    auto* appDesc = new QLabel(tr("Gán chế độ gõ riêng cho từng ứng dụng. Chế độ \"preedit\" dùng gạch chân (ổn định "
                                   "cho Chrome/Electron), \"direct\" gõ trực tiếp, \"off\" tắt bộ gõ:"), appPage);
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
    prepare_page_layout(backupLayout);

    auto* bakDesc = new QLabel(tr("Xuất/nhập toàn bộ cấu hình dạng file JSON, chọn lọc từng thành phần:"), backupPage);
    bakDesc->setWordWrap(true);
    backupLayout->addWidget(bakDesc);

    auto* bakGroup = new QGroupBox(tr("Thành phần sao lưu"), backupPage);
    auto* bakGroupLayout = new QVBoxLayout(bakGroup);
    m_bakMainSettings = new QCheckBox(tr("Main Settings (Cấu hình chung)"), bakGroup);
    m_bakMacros = new QCheckBox(tr("Macros (Gõ tắt và Emoji)"), bakGroup);
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
    interfaceLayout->setContentsMargins(24, 20, 24, 20);
    interfaceLayout->setVerticalSpacing(14);
    interfaceLayout->addRow(tr("Toggle key (Phím chuyển VI/EN)"), m_toggleKey);
    interfaceLayout->addRow(QString{}, m_showTrayIcon);
    interfaceLayout->addRow(QString{}, m_startWithSystem);

    // ==========================================
    // 6. DIAGNOSTICS PAGE (Chẩn đoán)
    // ==========================================
    auto* diagPage = new QWidget(this);
    auto* diagLayout = new QVBoxLayout(diagPage);
    prepare_page_layout(diagLayout);

    m_diagSummary = new QLabel(diagPage);
    m_diagSummary->setWordWrap(true);
    m_diagSummary->setTextInteractionFlags(Qt::TextSelectableByMouse);
    diagLayout->addWidget(m_diagSummary);

    m_diagReport = new QPlainTextEdit(diagPage);
    m_diagReport->setReadOnly(true);
    m_diagReport->setLineWrapMode(QPlainTextEdit::NoWrap);
    m_diagReport->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
    diagLayout->addWidget(m_diagReport, 1);

    auto* diagButtons = new QHBoxLayout();
    auto* diagRefresh = new QPushButton(tr("Làm mới"), diagPage);
    auto* diagCopy = new QPushButton(tr("Sao chép báo cáo"), diagPage);
    connect(diagRefresh, &QPushButton::clicked, this, &SettingsDialog::refreshDiagnostics);
    connect(diagCopy, &QPushButton::clicked, this, &SettingsDialog::copyDiagnostics);
    diagButtons->addWidget(diagRefresh);
    diagButtons->addWidget(diagCopy);
    diagButtons->addStretch();
    diagLayout->addLayout(diagButtons);

    m_diagTimer = new QTimer(this);
    m_diagTimer->setInterval(1500);
    connect(m_diagTimer, &QTimer::timeout, this, [this] {
        if (isVisible() && m_sidebar != nullptr && m_sidebar->currentRow() == m_diagIndex) {
            refreshDiagnostics();
        }
    });
    m_diagTimer->start();

    // ==========================================
    // 7. ABOUT PAGE (Giới thiệu)
    // ==========================================
    auto* aboutPage = new QWidget(this);
    auto* aboutLayout = new QVBoxLayout(aboutPage);
    aboutLayout->setContentsMargins(24, 24, 24, 24);
    aboutLayout->setSpacing(12);
    aboutLayout->addStretch();

    auto* logo = new QLabel(aboutPage);
    logo->setPixmap(QIcon(QStringLiteral(":/icons/nexkey.svg")).pixmap(112, 112));
    logo->setAlignment(Qt::AlignCenter);
    aboutLayout->addWidget(logo);

    auto* title = new QLabel(QStringLiteral("NexKey"), aboutPage);
    title->setObjectName(QStringLiteral("aboutTitle"));
    title->setAlignment(Qt::AlignCenter);
    aboutLayout->addWidget(title);

    auto* badge = new QLabel(
        tr("Phiên bản %1 (Stable)").arg(nexkey::gui::to_qstring(nexkey::kVersion)), aboutPage);
    badge->setObjectName(QStringLiteral("versionBadge"));
    badge->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    aboutLayout->addWidget(badge, 0, Qt::AlignHCenter);
    aboutLayout->addSpacing(10);

    auto* about = new QLabel(tr("Bộ gõ tiếng Việt thông minh cho Linux.\n"
                                "Gõ trực tiếp (Non-preedit, không gạch chân).\n"
                                "Hỗ trợ Uinput, Wayland & X11, App Rules, Macros, Smart Emoji."),
                             aboutPage);
    about->setAlignment(Qt::AlignCenter);
    about->setWordWrap(true);
    aboutLayout->addWidget(about);

    auto* repoLink = new QLabel(QStringLiteral("<a href=\"%1\">%1</a>").arg(kRepoUrl), aboutPage);
    repoLink->setAlignment(Qt::AlignCenter);
    repoLink->setTextFormat(Qt::RichText);
    repoLink->setOpenExternalLinks(true);
    aboutLayout->addWidget(repoLink);

    auto* linkRow = new QHBoxLayout();
    linkRow->addStretch();
    auto* bugBtn = new QPushButton(tr("Báo cáo lỗi"), aboutPage);
    auto* featureBtn = new QPushButton(tr("Đề xuất tính năng"), aboutPage);
    bugBtn->setMinimumWidth(180);
    featureBtn->setMinimumWidth(180);
    connect(bugBtn, &QPushButton::clicked, this, [] {
        QDesktopServices::openUrl(QUrl(kRepoUrl + QStringLiteral("/issues/new")));
    });
    connect(featureBtn, &QPushButton::clicked, this, [] {
        QDesktopServices::openUrl(QUrl(kRepoUrl + QStringLiteral("/issues/new?labels=enhancement")));
    });
    linkRow->addWidget(bugBtn);
    linkRow->addWidget(featureBtn);
    linkRow->addStretch();
    aboutLayout->addLayout(linkRow);

    auto* line = new QFrame(aboutPage);
    line->setFrameShape(QFrame::HLine);
    line->setFrameShadow(QFrame::Plain);
    aboutLayout->addSpacing(8);
    aboutLayout->addWidget(line);
    aboutLayout->addSpacing(4);

    auto* devBy = new QLabel(tr("Được phát triển bởi"), aboutPage);
    devBy->setAlignment(Qt::AlignCenter);
    aboutLayout->addWidget(devBy);

    auto* orgLink = new QLabel(
        QStringLiteral("<a href=\"%1\">Hyggshi OS Research Technology</a>").arg(kOrgUrl), aboutPage);
    orgLink->setAlignment(Qt::AlignCenter);
    orgLink->setTextFormat(Qt::RichText);
    orgLink->setOpenExternalLinks(true);
    aboutLayout->addWidget(orgLink);

    auto* configPath = new QLabel(
        tr("Đường dẫn file cấu hình:\n%1")
            .arg(QString::fromStdString(nexkey::default_config_path().string())),
        aboutPage);
    configPath->setAlignment(Qt::AlignCenter);
    configPath->setWordWrap(true);
    configPath->setTextInteractionFlags(Qt::TextSelectableByMouse);
    aboutLayout->addSpacing(10);
    aboutLayout->addWidget(configPath);
    aboutLayout->addStretch();

    // ==========================================
    // SIDEBAR + PAGES
    // ==========================================
    m_sidebar = new QListWidget(this);
    m_sidebar->setObjectName(QStringLiteral("sidebar"));
    m_sidebar->setIconSize(QSize(30, 30));
    m_sidebar->setFixedWidth(230);
    m_sidebar->setFocusPolicy(Qt::NoFocus);
    m_sidebar->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_sidebar->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    m_pages = new QStackedWidget(this);

    struct Entry {
        QString label;
        QWidget* page;
        QIcon icon;
    };
    const QStyle* st = style();
    const std::vector<Entry> entries = {
        {tr("Kiểu gõ"), inputPage,
         themed_icon({QStringLiteral("input-keyboard"), QStringLiteral("preferences-desktop-keyboard")},
                     st, QStyle::SP_FileDialogDetailedView)},
        {tr("Tính năng"), enginePage,
         themed_icon({QStringLiteral("preferences-system"), QStringLiteral("applications-system")}, st,
                     QStyle::SP_ComputerIcon)},
        {tr("Ứng dụng"), appPage,
         themed_icon({QStringLiteral("view-app-grid-symbolic"), QStringLiteral("applications-all"),
                      QStringLiteral("applications-other")},
                     st, QStyle::SP_DesktopIcon)},
        {tr("Sao lưu"), backupPage,
         themed_icon({QStringLiteral("document-save"), QStringLiteral("drive-harddisk")}, st,
                     QStyle::SP_DialogSaveButton)},
        {tr("Giao diện"), interfacePage,
         themed_icon({QStringLiteral("preferences-desktop-theme"),
                      QStringLiteral("preferences-desktop-color")},
                     st, QStyle::SP_DesktopIcon)},
        {tr("Chẩn đoán"), diagPage,
         themed_icon({QStringLiteral("utilities-system-monitor"), QStringLiteral("system-search"),
                      QStringLiteral("dialog-warning")},
                     st, QStyle::SP_FileDialogInfoView)},
        {tr("Giới thiệu"), aboutPage,
         themed_icon({QStringLiteral("help-about"), QStringLiteral("dialog-information")}, st,
                     QStyle::SP_MessageBoxInformation)},
    };
    for (const auto& entry : entries) {
        if (entry.page == diagPage) {
            m_diagIndex = m_sidebar->count();
        }
        auto* item = new QListWidgetItem(entry.icon, entry.label, m_sidebar);
        item->setSizeHint(QSize(0, 52));
        m_pages->addWidget(wrap_page(entry.page, m_pages));
    }
    connect(m_sidebar, &QListWidget::currentRowChanged, m_pages, &QStackedWidget::setCurrentIndex);
    connect(m_sidebar, &QListWidget::currentRowChanged, this, [this](int row) {
        if (row == m_diagIndex) {
            refreshDiagnostics();
        }
    });
    m_sidebar->setCurrentRow(0);

    // ==========================================
    // BOTTOM BAR: Cài đặt lại | Huỷ | Áp dụng | OK
    // ==========================================
    auto* resetBtn = new QPushButton(
        themed_icon({QStringLiteral("edit-undo"), QStringLiteral("view-refresh")}, st,
                    QStyle::SP_BrowserReload),
        tr("Cài đặt lại"), this);
    auto* cancelBtn = new QPushButton(
        themed_icon({QStringLiteral("dialog-cancel"), QStringLiteral("process-stop")}, st,
                    QStyle::SP_DialogCancelButton),
        tr("Huỷ"), this);
    auto* applyBtn = new QPushButton(
        themed_icon({QStringLiteral("dialog-apply"), QStringLiteral("dialog-ok-apply")}, st,
                    QStyle::SP_DialogApplyButton),
        tr("Áp dụng"), this);
    auto* okBtn = new QPushButton(
        themed_icon({QStringLiteral("dialog-ok"), QStringLiteral("emblem-ok-symbolic")}, st,
                    QStyle::SP_DialogOkButton),
        tr("OK"), this);
    okBtn->setObjectName(QStringLiteral("okButton"));
    okBtn->setDefault(true);

    connect(resetBtn, &QPushButton::clicked, this, &SettingsDialog::resetToDefaults);
    connect(cancelBtn, &QPushButton::clicked, this, &QDialog::reject);
    connect(applyBtn, &QPushButton::clicked, this, [this] { apply(); });
    connect(okBtn, &QPushButton::clicked, this, [this] {
        if (apply()) {
            accept();
        }
    });

    auto* bottom = new QHBoxLayout();
    bottom->addWidget(resetBtn);
    bottom->addStretch();
    bottom->addWidget(cancelBtn);
    bottom->addWidget(applyBtn);
    bottom->addWidget(okBtn);

    auto* body = new QHBoxLayout();
    body->setSpacing(12);
    body->addWidget(m_sidebar);
    body->addWidget(m_pages, 1);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(12, 12, 12, 12);
    layout->setSpacing(12);
    layout->addLayout(body, 1);
    layout->addLayout(bottom);

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

bool SettingsDialog::apply()
{
    m_settings = settings();
    if (!nexkey::save_settings(m_settings)) {
        QMessageBox::warning(this, tr("NexKey"),
                             tr("The configuration file could not be written:\n%1")
                                 .arg(QString::fromStdString(nexkey::default_config_path().string())));
        return false;
    }
    nexkey::gui::apply_autostart(m_settings.start_with_system);
    emit saved(m_settings);
    return true;
}

void SettingsDialog::resetToDefaults()
{
    const auto answer = QMessageBox::question(
        this, tr("Cài đặt lại"),
        tr("Đưa toàn bộ tuỳ chọn về mặc định?\nGõ tắt và từ điển cá nhân được giữ nguyên. "
           "Thay đổi chỉ có hiệu lực sau khi bấm Áp dụng hoặc OK."));
    if (answer != QMessageBox::Yes) {
        return;
    }
    nexkey::Settings defaults;
    defaults.enabled = m_settings.enabled;
    defaults.macros = m_settings.macros;
    defaults.custom_dictionary = m_settings.custom_dictionary;
    load(defaults);
}

void SettingsDialog::refreshDiagnostics()
{
    const auto report = nexkey::gui::collect_diagnostics();

    if (report.issues.isEmpty()) {
        m_diagSummary->setStyleSheet(QStringLiteral("color: #4caf50; font-weight: 600;"));
        m_diagSummary->setText(tr("✔ Không phát hiện vấn đề."));
    } else {
        QString text = tr("⚠ Phát hiện %1 vấn đề:").arg(report.issues.size());
        for (const auto& issue : report.issues) {
            text += QStringLiteral("\n• ") + issue;
        }
        m_diagSummary->setStyleSheet(QStringLiteral("color: #ffa726; font-weight: 600;"));
        m_diagSummary->setText(text);
    }

    // Keep the scroll position and any selection when nothing changed.
    if (m_diagReport->toPlainText() != report.text) {
        auto* bar = m_diagReport->verticalScrollBar();
        const int position = bar->value();
        m_diagReport->setPlainText(report.text);
        bar->setValue(position);
    }
}

void SettingsDialog::copyDiagnostics()
{
    refreshDiagnostics();
    QGuiApplication::clipboard()->setText(m_diagReport->toPlainText());
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
                         QStringLiteral("preedit"), QStringLiteral("direct"),
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
