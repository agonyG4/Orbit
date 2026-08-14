#pragma once

#include <QString>
#include <QStringList>

namespace Astrea::Explorer::Native::Services {

class DesktopFileId final
{
public:
    static QString fromPath(
        const QString &desktopFile,
        const QStringList &applicationRoots = {});
    static QString normalize(const QString &desktopId);
    static QStringList applicationRoots();
};

} // namespace Astrea::Explorer::Native::Services
