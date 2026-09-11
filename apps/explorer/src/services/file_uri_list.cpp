#include "services/file_uri_list.h"

#include <QUrl>

namespace Astrea::Explorer::Native::Services {

QByteArray fileUriListData(const QStringList &paths)
{
    QByteArray data;
    for (const QString &path : paths) {
        if (path.isEmpty()) {
            continue;
        }
        data.append(QUrl::fromLocalFile(path).toEncoded(QUrl::FullyEncoded));
        data.append("\r\n");
    }
    return data;
}

} // namespace Astrea::Explorer::Native::Services
