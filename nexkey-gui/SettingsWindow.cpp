#include "SettingsWindow.hpp"

#include <QButtonGroup>
#include <QCheckBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QRadioButton>
#include <QSettings>
#include <QTabWidget>
#include <QVBoxLayout>

namespace {

QWidget* radio_tab(QButtonGroup*& group, const QStringList& labels)
{
    auto* page = new QWidget;
    auto* layout = new QVBoxLayout(page);
    group = new QButtonGroup(page);
    for (int i = 0; i < labels.size(); ++i) {
        auto* button = new QRadioButton(labels[i], page);
        group->addButton(button, i);
        layout->addWidget(button);
    }
    layout->addStretch();
    return page;
}

} // namespace

SettingsWindow::SettingsWindow(QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle("NexKey Settings");
    resize(480, 340);

    auto* tabs = new QTabWidget(this);
    tabs->addTab(radio_tab(m_methodGroup, {"Telex", "VNI", "VIQR"}), "Input Method");
    tabs->addTab(radio_tab(m_encodingGroup, {"Unicode", "TCVN3", "VNI encoding"}), "Output Encoding");

    auto* options = new QWidget(this);
    auto* optionsLayout = new QVBoxLayout(options);
    m_autostart = new QCheckBox("Start with system", options);
    m_showTray = new QCheckBox("Show tray icon", options);
    m_smartBackspace = new QCheckBox("Smart backspace", options);
    m_processAtWordEnd = new QCheckBox("Process at word end", options);
    optionsLayout->addWidget(m_autostart);
    optionsLayout->addWidget(m_showTray);
    optionsLayout->addWidget(m_smartBackspace);
    optionsLayout->addWidget(m_processAtWordEnd);
    optionsLayout->addStretch();
    tabs->addTab(options, "Options");

    auto* shortcuts = new QWidget(this);
    auto* shortcutLayout = new QFormLayout(shortcuts);
    m_shortcut = new QLineEdit("Ctrl+Shift", shortcuts);
    shortcutLayout->addRow("Toggle hotkey", m_shortcut);
    tabs->addTab(shortcuts, "Shortcuts");

    auto* about = new QWidget(this);
    auto* aboutLayout = new QVBoxLayout(about);
    aboutLayout->addWidget(new QLabel("NexKey 1.0.0\nMIT License\nhttps://github.com/Hyggshi-OS-project-center/NexKey", about));
    aboutLayout->addStretch();
    tabs->addTab(about, "About");

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::accepted, this, [this] {
        save();
        accept();
    });
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    auto* layout = new QVBoxLayout(this);
    layout->addWidget(tabs);
    layout->addWidget(buttons);

    load();
}

void SettingsWindow::load()
{
    QSettings settings(QSettings::IniFormat, QSettings::UserScope, "nexkey", "nexkey");
    m_methodGroup->button(settings.value("input/method", 0).toInt())->setChecked(true);
    m_encodingGroup->button(settings.value("output/encoding", 0).toInt())->setChecked(true);
    m_autostart->setChecked(settings.value("options/autostart", false).toBool());
    m_showTray->setChecked(settings.value("options/showTray", true).toBool());
    m_smartBackspace->setChecked(settings.value("options/smartBackspace", true).toBool());
    m_processAtWordEnd->setChecked(settings.value("options/processAtWordEnd", true).toBool());
    m_shortcut->setText(settings.value("shortcuts/toggle", "Ctrl+Shift").toString());
}

void SettingsWindow::save()
{
    QSettings settings(QSettings::IniFormat, QSettings::UserScope, "nexkey", "nexkey");
    settings.setValue("input/method", m_methodGroup->checkedId());
    settings.setValue("output/encoding", m_encodingGroup->checkedId());
    settings.setValue("options/autostart", m_autostart->isChecked());
    settings.setValue("options/showTray", m_showTray->isChecked());
    settings.setValue("options/smartBackspace", m_smartBackspace->isChecked());
    settings.setValue("options/processAtWordEnd", m_processAtWordEnd->isChecked());
    settings.setValue("shortcuts/toggle", m_shortcut->text());
}
