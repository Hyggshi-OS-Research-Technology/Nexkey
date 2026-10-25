// SPDX-License-Identifier: MIT
// NexKey - D-Bus service.

#include "Daemon.hpp"

#include "Autostart.hpp"
#include "QtHelpers.hpp"

#include <QTimer>

#include <string>

Daemon::Daemon(QObject* parent)
    : QObject(parent)
    , m_config_path(nexkey::default_config_path())
{
    reload();
    // Keep the autostart entry in sync with the configuration, so editing the
    // file by hand has the same effect as using the settings dialog.
    nexkey::gui::apply_autostart(m_settings.start_with_system);

    auto* watch = new QTimer(this);
    watch->setInterval(1000);
    connect(watch, &QTimer::timeout, this, [this] {
        std::error_code error;
        const auto modified = std::filesystem::last_write_time(m_config_path, error);
        if (error) {
            return;
        }
        if (modified != m_config_time) {
            reload();
        }
    });
    watch->start();
}

QString Daemon::mode() const
{
    return m_settings.enabled ? QStringLiteral("VI") : QStringLiteral("EN");
}

QString Daemon::method() const
{
    return nexkey::gui::to_qstring(nexkey::to_string(m_settings.options.method));
}

QString Daemon::encoding() const
{
    return nexkey::gui::to_qstring(nexkey::to_string(m_settings.options.encoding));
}

QString Daemon::GetMode() const
{
    return mode();
}

bool Daemon::GetEnabled() const
{
    return m_settings.enabled;
}

void Daemon::setEnabled(bool enabled)
{
    if (m_settings.enabled == enabled) {
        return;
    }
    m_settings.enabled = enabled;
    if (!nexkey::save_settings(m_settings)) {
        qWarning("NexKey: cannot write the configuration file");
    }
    emit modeChanged(mode());
}

void Daemon::setSettings(const nexkey::Settings& settings, bool persist)
{
    m_settings = settings;
    if (persist && !nexkey::save_settings(m_settings)) {
        qWarning("NexKey: cannot write the configuration file");
    }
    reload();
}

void Daemon::Toggle()
{
    setEnabled(!m_settings.enabled);
}

void Daemon::SetEnabled(bool enabled)
{
    setEnabled(enabled);
}

void Daemon::Reload()
{
    reload();
}

void Daemon::ShowSettings()
{
    emit settingsRequested();
}

void Daemon::reload()
{
    std::error_code error;
    const auto modified = std::filesystem::last_write_time(m_config_path, error);
    if (!error) {
        m_config_time = modified;
    }

    const bool mode_was = m_settings.enabled;
    const bool autostart_was = m_settings.start_with_system;
    m_settings = nexkey::load_settings(m_config_path);

    if (autostart_was != m_settings.start_with_system) {
        nexkey::gui::apply_autostart(m_settings.start_with_system);
    }
    if (mode_was != m_settings.enabled) {
        emit modeChanged(mode());
    }
    emit settingsChanged();
}
