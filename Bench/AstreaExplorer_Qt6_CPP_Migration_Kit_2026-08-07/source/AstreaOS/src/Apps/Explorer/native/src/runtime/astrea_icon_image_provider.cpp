#include "astrea_icon_image_provider.h"

#include <QThread>
#include <QUrl>

#include "services/icon_theme_service.h"

namespace Astrea::Explorer::Native::Runtime {

AstreaIconImageProvider::AstreaIconImageProvider(Services::IconThemeService *service)
    : QQuickImageProvider(QQuickImageProvider::Image)
    , m_service(service)
{
}

QImage AstreaIconImageProvider::requestImage(
    const QString &id,
    QSize *size,
    const QSize &requestedSize)
{
    if (!m_service) {
        if (size) {
            *size = requestedSize.isValid() ? requestedSize : QSize(32, 32);
        }
        return {};
    }

    // Theme icons are intentionally requested synchronously by QML. This keeps
    // QIcon/QPixmap on the GUI thread; thumbnail providers remain asynchronous.
    Q_ASSERT(QThread::currentThread() == m_service->thread());

    QString payload = id;
    const int queryIndex = payload.indexOf(QLatin1Char('?'));
    if (queryIndex >= 0) {
        payload.truncate(queryIndex);
    }
    if (payload.startsWith(QStringLiteral("theme/"))) {
        payload.remove(0, 6);
    }

    const QString decoded = QUrl::fromPercentEncoding(payload.toUtf8());
    const QStringList candidates = decoded.isEmpty()
        ? QStringList{}
        : decoded.split(QLatin1Char('|'), Qt::SkipEmptyParts);
    const QSize logicalSize = requestedSize.isValid() ? requestedSize : QSize(32, 32);
    const QImage image = m_service->renderIcon(candidates, logicalSize, 1.0);
    if (size) {
        *size = image.size();
    }
    return image;
}

} // namespace Astrea::Explorer::Native::Runtime
