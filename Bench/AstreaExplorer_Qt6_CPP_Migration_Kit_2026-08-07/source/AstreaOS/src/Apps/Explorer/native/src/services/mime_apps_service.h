#pragma once

#include <QString>
#include <QStringList>

#include "services/xdg_paths.h"

namespace Astrea::Explorer::Native::Services {

class MimeAppsService final
{
public:
    explicit MimeAppsService(
        QString filePath = {},
        int lockTimeoutMs = 5000,
        XdgPaths paths = XdgPaths::fromEnvironment());

    QStringList defaultsForMime(const QString &mime) const;
    QStringList associationsForMime(const QString &mime) const;
    bool setDefault(const QString &mime, const QString &desktopId) const;

    QString filePath() const;

private:
    QStringList readLines(const QString &path) const;
    QStringList valuesFor(
        const QStringList &lines,
        const QString &section,
        const QString &mime) const;
    QStringList effectiveAssociations(const QString &mime) const;
    QStringList validDesktopIds(const QStringList &ids) const;
    bool isValidDesktopId(const QString &desktopId) const;
    QStringList searchPaths() const;
    bool updateValue(
        QStringList *lines,
        const QString &section,
        const QString &mime,
        const QStringList &values) const;

    QString m_filePath;
    int m_lockTimeoutMs = 5000;
    XdgPaths m_paths;
    bool m_filePathWasExplicit = false;
};

} // namespace Astrea::Explorer::Native::Services
