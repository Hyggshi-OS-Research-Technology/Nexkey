// SPDX-License-Identifier: MIT
// NexKey - settings dialog.

#include "SettingsDialog.hpp"

#include "Autostart.hpp"
#include "QtHelpers.hpp"

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QIcon>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QTabWidget>
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
    resize(460, 380);

    m_method = new QComboBox(this);
    fill_combo(m_method, {{tr("Telex"), static_cast<int>(nexkey::Method::Telex)},
                          {tr("Simple Telex"), static_cast<int>(nexkey::Method::SimpleTelex)},
                          {tr("VNI"), static_cast<int>(nexkey::Method::Vni)},
                          {tr("VIQR"), static_cast<int>(nexkey::Method::Viqr)}});

    m_toneStyle = new QComboBox(this);
    fill_combo(m_toneStyle,
               {{tr("Modern (hoa with the tone on a)"), static_cast<int>(nexkey::ToneStyle::Modern)},
                {tr("Traditional (hoa with the tone on o)"),
                 static_cast<int>(nexkey::ToneStyle::Traditional)}});

    m_encoding = new QComboBox(this);
    fill_combo(m_encoding, {{tr("Unicode"), static_cast<int>(nexkey::Encoding::Unicode)},
                            {tr("Unicode decomposed (NFD)"),
                             static_cast<int>(nexkey::Encoding::UnicodeDecomposed)},
                            {tr("TCVN3 (ABC)"), static_cast<int>(nexkey::Encoding::Tcvn3)},
                            {tr("VIQR"), static_cast<int>(nexkey::Encoding::Viqr)}});

    auto* inputPage = new QWidget(this);
    auto* inputLayout = new QFormLayout(inputPage);
    inputLayout->addRow(tr("Input method"), m_method);
    inputLayout->addRow(tr("Tone placement"), m_toneStyle);
    inputLayout->addRow(tr("Output charset"), m_encoding);

    m_smartBackspace = new QCheckBox(tr("Smart backspace"), this);
    m_englishFallback = new QCheckBox(tr("Keep non-Vietnamese words as typed"), this);
    m_bracketShortcuts = new QCheckBox(tr("Telex bracket shortcuts for the horned vowels"), this);
    m_candidates = new QCheckBox(tr("Show the word selection window"), this);

    auto* enginePage = new QWidget(this);
    auto* engineLayout = new QVBoxLayout(enginePage);
    engineLayout->addWidget(m_smartBackspace);
    engineLayout->addWidget(m_englishFallback);
    engineLayout->addWidget(m_bracketShortcuts);
    engineLayout->addWidget(m_candidates);
    engineLayout->addStretch();

    m_toggleKey = new QLineEdit(this);
    m_toggleKey->setPlaceholderText(QStringLiteral("Control+Shift"));
    m_showTrayIcon = new QCheckBox(tr("Show the tray icon"), this);
    m_startWithSystem = new QCheckBox(tr("Start automatically when I sign in"), this);

    auto* interfacePage = new QWidget(this);
    auto* interfaceLayout = new QFormLayout(interfacePage);
    interfaceLayout->addRow(tr("Toggle key"), m_toggleKey);
    interfaceLayout->addRow(QString{}, m_showTrayIcon);
    interfaceLayout->addRow(QString{}, m_startWithSystem);

    auto* aboutPage = new QWidget(this);
    auto* aboutLayout = new QVBoxLayout(aboutPage);
    auto* about = new QLabel(tr("NexKey %1\n"
                               "Vietnamese input method for Linux.\n"
                               "MIT licensed.\n\n"
                               "The configuration file lives in\n%2")
                                 .arg(nexkey::gui::to_qstring(nexkey::kVersion),
                                      QString::fromStdString(nexkey::default_config_path().string())),
                            aboutPage);
    about->setWordWrap(true);
    aboutLayout->addWidget(about);
    aboutLayout->addStretch();

    auto* tabs = new QTabWidget(this);
    tabs->addTab(inputPage, tr("Input"));
    tabs->addTab(enginePage, tr("Engine"));
    tabs->addTab(interfacePage, tr("Interface"));
    tabs->addTab(aboutPage, tr("About"));

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

void SettingsDialog::load(const nexkey::Settings& settings)
{
    m_settings = settings;
    m_method->setCurrentIndex(m_method->findData(static_cast<int>(settings.options.method)));
    m_toneStyle->setCurrentIndex(
        m_toneStyle->findData(static_cast<int>(settings.options.tone_style)));
    m_encoding->setCurrentIndex(m_encoding->findData(static_cast<int>(settings.options.encoding)));
    m_smartBackspace->setChecked(settings.options.smart_backspace);
    m_englishFallback->setChecked(settings.options.english_fallback);
    m_bracketShortcuts->setChecked(settings.options.bracket_shortcuts);
    m_candidates->setChecked(settings.options.candidates);
    m_showTrayIcon->setChecked(settings.show_tray_icon);
    m_startWithSystem->setChecked(settings.start_with_system);
    m_toggleKey->setText(QString::fromStdString(settings.toggle_key));
}

nexkey::Settings SettingsDialog::settings() const
{
    nexkey::Settings result = m_settings;
    result.options.method = static_cast<nexkey::Method>(m_method->currentData().toInt());
    result.options.tone_style = static_cast<nexkey::ToneStyle>(m_toneStyle->currentData().toInt());
    result.options.encoding = static_cast<nexkey::Encoding>(m_encoding->currentData().toInt());
    result.options.smart_backspace = m_smartBackspace->isChecked();
    result.options.english_fallback = m_englishFallback->isChecked();
    result.options.bracket_shortcuts = m_bracketShortcuts->isChecked();
    result.options.candidates = m_candidates->isChecked();
    result.show_tray_icon = m_showTrayIcon->isChecked();
    result.start_with_system = m_startWithSystem->isChecked();
    result.toggle_key = m_toggleKey->text().toStdString();
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
