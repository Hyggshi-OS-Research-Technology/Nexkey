// SPDX-License-Identifier: MIT
// NexKey - settings dialog.

#pragma once

#include "nexkey/Settings.hpp"

#include <QDialog>

class QCheckBox;
class QComboBox;
class QLineEdit;
class QLabel;
class QListWidget;
class QPlainTextEdit;
class QPushButton;
class QStackedWidget;
class QTableWidget;
class QTimer;

class SettingsDialog : public QDialog {
    Q_OBJECT

public:
    explicit SettingsDialog(const nexkey::Settings& settings, QWidget* parent = nullptr);

    [[nodiscard]] nexkey::Settings settings() const;

signals:
    void saved(const nexkey::Settings& settings);

private slots:
    void applyPreset(nexkey::Method method);
    void exportJson();
    void importJson();
    void addAppRule();
    void removeAppRule();
    void resetToDefaults();
    void refreshDiagnostics();
    void copyDiagnostics();

private:
    bool apply();
    void load(const nexkey::Settings& settings);

    // Layout: sidebar on the left, pages on the right.
    QListWidget* m_sidebar{};
    QStackedWidget* m_pages{};

    QComboBox* m_method{};
    QComboBox* m_toneStyle{};
    QComboBox* m_encoding{};
    QCheckBox* m_directTyping{};
    QCheckBox* m_smartBackspace{};
    QCheckBox* m_englishFallback{};
    QCheckBox* m_spellCheck{};
    QCheckBox* m_macroEnabled{};
    QCheckBox* m_capitalizeMacro{};
    QCheckBox* m_customDict{};
    QCheckBox* m_bracketShortcuts{};
    QCheckBox* m_candidates{};

    QLineEdit* m_timeFormat{};
    QLineEdit* m_dateFormat{};

    QCheckBox* m_showTrayIcon{};
    QCheckBox* m_startWithSystem{};
    QLineEdit* m_toggleKey{};

    // App Rules
    QTableWidget* m_appRulesTable{};
    QPushButton* m_addRuleBtn{};
    QPushButton* m_removeRuleBtn{};

    // Backup & Restore
    QCheckBox* m_bakMainSettings{};
    QCheckBox* m_bakMacros{};
    QCheckBox* m_bakKeymaps{};
    QCheckBox* m_bakAppRules{};
    QCheckBox* m_bakCustomDict{};
    QPushButton* m_exportBtn{};
    QPushButton* m_importBtn{};

    // Diagnostics
    QLabel* m_diagSummary{};
    QPlainTextEdit* m_diagReport{};
    QTimer* m_diagTimer{};
    int m_diagIndex{-1};

    nexkey::Settings m_settings;
};
