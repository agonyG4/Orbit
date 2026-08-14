#pragma once

#include <QString>
#include <QStringList>

namespace Astrea::Explorer::Native::Services {

class MimeAppsService final
{
public:
    explicit MimeAppsService(QString filePath = {}, int lockTimeoutMs = 5000);

    QStringList defaultsForMime(const QString &mime) const;
    QStringList associationsForMime(const QString &mime) const;
    bool setDefault(const QString &mime, const QString &desktopId) const;

    QString filePath() const;

private:
    QStringList readLines() const;
    QStringList valuesFor(
        const QStringList &lines,
        const QString &section,
        const QString &mime) const;
    bool updateValue(
        QStringList *lines,
        const QString &section,
        const QString &mime,
        const QStringList &values) const;

    QString m_filePath;
    int m_lockTimeoutMs = 5000;
};

} // namespace Astrea::Explorer::Native::Services
