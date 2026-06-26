#include "DBusService.hpp"

DBusService::DBusService(QObject* parent)
    : QObject(parent)
{
}

QString DBusService::GetMode() const { return m_mode; }

void DBusService::SetMode(const QString& mode)
{
    if (m_mode == mode) {
        return;
    }
    m_mode = mode;
    emit ModeChanged(m_mode);
}

QString DBusService::GetMethod() const { return m_method; }
void DBusService::SetMethod(const QString& method) { m_method = method; }
QString DBusService::GetEncoding() const { return m_encoding; }
void DBusService::SetEncoding(const QString& encoding) { m_encoding = encoding; }
