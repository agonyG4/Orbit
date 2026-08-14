#pragma once

#include <QDir>
#include <QProcessEnvironment>
#include <QString>
#include <QStringList>

namespace Astrea::Explorer::Native::Services {

struct XdgPaths
{
    QString home;
    QString configHome;
    QStringList configDirs;
    QString dataHome;
    QStringList dataDirs;
    QString stateHome;
    QString cacheHome;
    QString currentDesktop;

    static XdgPaths fromEnvironment(
        const QProcessEnvironment &environment = QProcessEnvironment::systemEnvironment())
    {
        const QString suppliedHome = environment.value(QStringLiteral("HOME")).trimmed();
        const QString home = suppliedHome.isEmpty() ? QDir::homePath() : suppliedHome;
        const auto valueOr = [&environment](const QString &name, const QString &fallback) {
            const QString value = environment.value(name).trimmed();
            return value.isEmpty() ? fallback : value;
        };
        const auto listOr = [&environment](const QString &name, const QStringList &fallback) {
            const QString value = environment.value(name).trimmed();
            if (value.isEmpty()) {
                return fallback;
            }
            return value.split(QLatin1Char(':'), Qt::SkipEmptyParts);
        };

        return {
            home,
            valueOr(QStringLiteral("XDG_CONFIG_HOME"), QDir(home).filePath(QStringLiteral(".config"))),
            listOr(QStringLiteral("XDG_CONFIG_DIRS"), {QStringLiteral("/etc/xdg")}),
            valueOr(QStringLiteral("XDG_DATA_HOME"), QDir(home).filePath(QStringLiteral(".local/share"))),
            listOr(QStringLiteral("XDG_DATA_DIRS"), {QStringLiteral("/usr/local/share"), QStringLiteral("/usr/share")}),
            valueOr(QStringLiteral("XDG_STATE_HOME"), QDir(home).filePath(QStringLiteral(".local/state"))),
            valueOr(QStringLiteral("XDG_CACHE_HOME"), QDir(home).filePath(QStringLiteral(".cache"))),
            environment.value(QStringLiteral("XDG_CURRENT_DESKTOP")).trimmed(),
        };
    }

    QStringList applicationRoots() const
    {
        QStringList roots;
        const auto appendUnique = [&roots](const QString &root) {
            const QString normalized = QDir::cleanPath(root);
            if (!normalized.isEmpty() && !roots.contains(normalized)) {
                roots.append(normalized);
            }
        };
        appendUnique(QDir(dataHome).filePath(QStringLiteral("applications")));
        for (const QString &dataDir : dataDirs) {
            appendUnique(QDir(dataDir).filePath(QStringLiteral("applications")));
        }
        return roots;
    }

    QStringList mimeAppsSearchPaths() const
    {
        QStringList paths;
        const QString desktop = currentDesktop.split(QLatin1Char(':'), Qt::SkipEmptyParts)
                                    .value(0).trimmed();
        const auto appendLocation = [&paths, &desktop](const QString &directory) {
            if (directory.trimmed().isEmpty()) {
                return;
            }
            if (!desktop.isEmpty()) {
                paths.append(QDir(directory).filePath(desktop + QStringLiteral("-mimeapps.list")));
            }
            paths.append(QDir(directory).filePath(QStringLiteral("mimeapps.list")));
        };
        appendLocation(configHome);
        for (const QString &configDir : configDirs) {
            appendLocation(configDir);
        }
        appendLocation(QDir(dataHome).filePath(QStringLiteral("applications")));
        for (const QString &dataDir : dataDirs) {
            appendLocation(QDir(dataDir).filePath(QStringLiteral("applications")));
        }
        return paths;
    }
};

} // namespace Astrea::Explorer::Native::Services
