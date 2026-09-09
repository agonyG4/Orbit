#include "astrea_icon_image_provider.h"

#include <QFileInfo>
#include <QImageReader>
#include <QThread>
#include <QUrl>
#include <QUrlQuery>

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
    QString queryText;
    const int queryIndex = payload.indexOf(QLatin1Char('?'));
    if (queryIndex >= 0) {
        queryText = payload.mid(queryIndex + 1);
        payload.truncate(queryIndex);
    }
    const QUrlQuery query(queryText);
    const QSize logicalSize = requestedSize.isValid()
        ? requestedSize
        : QSize(query.queryItemValue(QStringLiteral("size")).toInt(),
                query.queryItemValue(QStringLiteral("size")).toInt());

    if (payload.startsWith(QStringLiteral("file/"))) {
        payload.remove(0, 5);
        const QUrl iconUrl(QUrl::fromPercentEncoding(payload.toUtf8()));
        if (iconUrl.isValid() && iconUrl.isLocalFile()) {
            const QFileInfo iconFile(iconUrl.toLocalFile());
            if (iconFile.isFile()) {
                QImageReader reader(iconFile.absoluteFilePath());
                reader.setAutoTransform(true);
                const QSize sourceSize = reader.size();
                if (sourceSize.isValid() && logicalSize.isValid()) {
                    reader.setScaledSize(sourceSize.scaled(logicalSize, Qt::KeepAspectRatio));
                }
                const QImage image = reader.read();
                if (!image.isNull()) {
                    if (size) {
                        *size = image.size();
                    }
                    return image;
                }
            }
        }

        const QString fallback = query.queryItemValue(QStringLiteral("fallback"));
        const QStringList candidates = fallback.isEmpty()
            ? QStringList{}
            : fallback.split(QLatin1Char('|'), Qt::SkipEmptyParts);
        const QImage image = m_service->renderIcon(candidates, logicalSize, 1.0);
        if (size) {
            *size = image.size();
        }
        return image;
    }
    if (payload.startsWith(QStringLiteral("theme/"))) {
        payload.remove(0, 6);
    }

    const QString decoded = QUrl::fromPercentEncoding(payload.toUtf8());
    const QStringList candidates = decoded.isEmpty()
        ? QStringList{}
        : decoded.split(QLatin1Char('|'), Qt::SkipEmptyParts);
    const QSize effectiveSize = logicalSize.isValid() ? logicalSize : QSize(32, 32);
    // The provider does not currently receive the window/device scale, so
    // explicit DPR-aware service callers remain the only HiDPI path.
    const QImage image = m_service->renderIcon(candidates, effectiveSize, 1.0);
    if (size) {
        *size = image.size();
    }
    return image;
}

} // namespace Astrea::Explorer::Native::Runtime
