#include "services/desktop_file_id.h"

#include <QDir>
#include <QFileInfo>

namespace Astrea::Explorer::Native::Services {

QString DesktopFileId::fromPath(
    const QString &desktopFile,
    const QStringList &applicationRoots)
{
    const QFileInfo fileInfo(desktopFile);
    if (!fileInfo.isFile() || !fileInfo.fileName().endsWith(QStringLiteral(".desktop"))) {
        return {};
    }

    const QStringList roots = applicationRoots.isEmpty()
        ? DesktopFileId::applicationRoots()
        : applicationRoots;
    const QString absoluteFile = QDir::cleanPath(fileInfo.absoluteFilePath());
    for (const QString &root : roots) {
        if (root.trimmed().isEmpty()) {
            continue;
        }
        const QString absoluteRoot = QDir::cleanPath(QFileInfo(root).absoluteFilePath());
        const QString relative = QDir(absoluteRoot).relativeFilePath(absoluteFile);
        if (relative == QStringLiteral(".")
            || relative.startsWith(QStringLiteral("../"))
            || relative == QStringLiteral("..")) {
            continue;
        }
        return normalize(relative);
    }

    return {};
}

QString DesktopFileId::normalize(const QString &desktopId)
{
    QString normalized = QDir::cleanPath(desktopId.trimmed());
    if (normalized.isEmpty() || normalized == QStringLiteral(".")) {
        return {};
    }
    normalized.replace(QLatin1Char('\\'), QLatin1Char('/'));
    while (normalized.startsWith(QStringLiteral("./"))) {
        normalized.remove(0, 2);
    }
    if (!normalized.endsWith(QStringLiteral(".desktop"))) {
        normalized += QStringLiteral(".desktop");
    }
    normalized.replace(QLatin1Char('/'), QLatin1Char('-'));
    return normalized;
}

QStringList DesktopFileId::applicationRoots()
{
    return applicationRoots(XdgPaths::fromEnvironment());
}

QStringList DesktopFileId::applicationRoots(const XdgPaths &paths)
{
    return paths.applicationRoots();
}

} // namespace Astrea::Explorer::Native::Services
