// SPDX-License-Identifier: MIT
// NexKey - D-Bus service that owns the shared configuration.

#pragma once

#include "nexkey/Settings.hpp"

#include <QObject>
#include <QString>

#include <filesystem>

/// Exposes the NexKey configuration on the session bus as
/// `org.nexkey.Daemon` at `/org/nexkey/Daemon`.
class Daemon : public QObject {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.nexkey.Daemon")
    Q_PROPERTY(QString Mode READ mode NOTIFY modeChanged)
    Q_PROPERTY(bool Enabled READ enabled WRITE setEnabled NOTIFY modeChanged)
    Q_PROPERTY(QString Method READ method NOTIFY settingsChanged)
    Q_PROPERTY(QString Encoding READ encoding NOTIFY settingsChanged)

public:
    explicit Daemon(QObject* parent = nullptr);

    [[nodiscard]] const nexkey::Settings& settings() const { return m_settings; }
    [[nodiscard]] QString mode() const;
    [[nodiscard]] bool enabled() const { return m_settings.enabled; }
    [[nodiscard]] QString method() const;
    [[nodiscard]] QString encoding() const;

    void setEnabled(bool enabled);
    void setSettings(const nexkey::Settings& settings, bool persist = true);

    /// Re-read the configuration file, for example after the CLI changed it.
    void reload();

signals:
    void modeChanged(const QString& mode);
    void settingsChanged();
    void settingsRequested();

public slots:
    Q_SCRIPTABLE QString GetMode() const;
    Q_SCRIPTABLE bool GetEnabled() const;
    Q_SCRIPTABLE void Toggle();
    Q_SCRIPTABLE void SetEnabled(bool enabled);
    Q_SCRIPTABLE void Reload();
    Q_SCRIPTABLE void ShowSettings();

private:
    nexkey::Settings m_settings;
    std::filesystem::path m_config_path;
    std::filesystem::file_time_type m_config_time{};
};
