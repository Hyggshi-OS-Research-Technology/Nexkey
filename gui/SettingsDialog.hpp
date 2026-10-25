// SPDX-License-Identifier: MIT
// NexKey - settings dialog.

#pragma once

#include "nexkey/Settings.hpp"

#include <QDialog>

class QCheckBox;
class QComboBox;
class QLineEdit;

class SettingsDialog : public QDialog {
    Q_OBJECT

public:
    explicit SettingsDialog(const nexkey::Settings& settings, QWidget* parent = nullptr);

    [[nodiscard]] nexkey::Settings settings() const;

signals:
    void saved(const nexkey::Settings& settings);

private:
    void apply();
    void load(const nexkey::Settings& settings);

    QComboBox* m_method{};
    QComboBox* m_toneStyle{};
    QComboBox* m_encoding{};
    QCheckBox* m_smartBackspace{};
    QCheckBox* m_englishFallback{};
    QCheckBox* m_bracketShortcuts{};
    QCheckBox* m_showTrayIcon{};
    QCheckBox* m_startWithSystem{};
    QLineEdit* m_toggleKey{};
    nexkey::Settings m_settings;
};
