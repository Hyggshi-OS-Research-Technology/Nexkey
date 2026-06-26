#pragma once

#include <QDialog>

class QButtonGroup;
class QCheckBox;
class QLineEdit;

class SettingsWindow : public QDialog {
    Q_OBJECT

public:
    explicit SettingsWindow(QWidget* parent = nullptr);

private:
    void load();
    void save();

    QButtonGroup* m_methodGroup{};
    QButtonGroup* m_encodingGroup{};
    QCheckBox* m_autostart{};
    QCheckBox* m_showTray{};
    QCheckBox* m_smartBackspace{};
    QCheckBox* m_processAtWordEnd{};
    QLineEdit* m_shortcut{};
};
