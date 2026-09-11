#pragma once

#include <QByteArray>
#include <QStringList>

namespace Astrea::Explorer::Native::Services {

QByteArray fileUriListData(const QStringList &paths);

} // namespace Astrea::Explorer::Native::Services
