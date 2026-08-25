#pragma once

#include <QString>
#include <QStringList>

#include "services/xdg_paths.h"

namespace Astrea::Explorer::Native::Services {

class DesktopFileId final
{
public:
    static QString fromPath(
        const QString &desktopFile,
        const QStringList &applicationRoots = {});
    static QString normalize(const QString &desktopId);
    static QStringList applicationRoots();
    static QStringList applicationRoots(const XdgPaths &paths);
};

} // namespace Astrea::Explorer::Native::Services
