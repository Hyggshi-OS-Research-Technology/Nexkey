#pragma once

#include <QObject>
#include <QString>

class DBusService : public QObject {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.nexkey.Settings")

public:
    explicit DBusService(QObject* parent = nullptr);

public slots:
    QString GetMode() const;
    void SetMode(const QString& mode);
    QString GetMethod() const;
    void SetMethod(const QString& method);
    QString GetEncoding() const;
    void SetEncoding(const QString& encoding);

signals:
    void ModeChanged(const QString& mode);

private:
    QString m_mode{"VI"};
    QString m_method{"telex"};
    QString m_encoding{"unicode"};
};
