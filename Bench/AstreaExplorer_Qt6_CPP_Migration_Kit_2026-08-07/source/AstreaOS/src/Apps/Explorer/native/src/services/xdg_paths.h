#pragma once

#include <QDir>
#include <QProcessEnvironment>
#include <QString>
#include <QStringList>
#include <QVector>

namespace Astrea::Explorer::Native::Services {

struct MimeAppsLocation
{
    QString path;
    bool desktopSpecific = false;
};

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

    QStringList desktopNames() const
    {
        QStringList names;
        for (const QString &raw : currentDesktop.split(QLatin1Char(':'), Qt::SkipEmptyParts)) {
            const QString name = raw.trimmed().toLower();
            if (!name.isEmpty() && !names.contains(name)) {
                names.append(name);
            }
        }
        return names;
    }

    QVector<MimeAppsLocation> mimeAppsSearchLocations() const
    {
        QVector<MimeAppsLocation> locations;
        const QStringList names = desktopNames();
        const auto appendLocation = [&locations, &names](const QString &directory) {
            if (directory.trimmed().isEmpty()) {
                return;
            }
            for (const QString &name : names) {
                locations.append({
                    QDir(directory).filePath(name + QStringLiteral("-mimeapps.list")),
                    true,
                });
            }
            locations.append({
                QDir(directory).filePath(QStringLiteral("mimeapps.list")),
                false,
            });
        };
        appendLocation(configHome);
        for (const QString &configDir : configDirs) {
            appendLocation(configDir);
        }
        appendLocation(QDir(dataHome).filePath(QStringLiteral("applications")));
        for (const QString &dataDir : dataDirs) {
            appendLocation(QDir(dataDir).filePath(QStringLiteral("applications")));
        }
        return locations;
    }

    QStringList mimeAppsSearchPaths() const
    {
        QStringList paths;
        for (const MimeAppsLocation &location : mimeAppsSearchLocations()) {
            paths.append(location.path);
        }
        return paths;
    }
};

} // namespace Astrea::Explorer::Native::Services
