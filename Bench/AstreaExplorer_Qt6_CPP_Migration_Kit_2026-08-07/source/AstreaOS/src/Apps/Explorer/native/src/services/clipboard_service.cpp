#include "services/clipboard_service.h"

#include <QBuffer>
#include <QClipboard>
#include <QUrl>
#include <QVariant>

namespace Astrea::Explorer::Native::Services {

ClipboardService::ClipboardService(QClipboard *clipboard)
    : m_clipboard(clipboard)
{
}

std::unique_ptr<QMimeData> ClipboardService::fileMimeData(const QStringList &paths) const
{
    QList<QUrl> urls;
    QByteArray uriList;
    QStringList textPaths;
    for (const QString &path : paths) {
        if (path.isEmpty()) {
            continue;
        }
        const QUrl url = QUrl::fromLocalFile(path);
        urls.append(url);
        uriList.append(url.toEncoded(QUrl::FullyEncoded));
        uriList.append("\r\n");
        textPaths.append(path);
    }
    if (urls.isEmpty()) {
        return nullptr;
    }

    auto mimeData = std::make_unique<QMimeData>();
    mimeData->setUrls(urls);
    mimeData->setData(QStringLiteral("text/uri-list"), uriList);
    mimeData->setText(textPaths.join(QLatin1Char('\n')));
    return mimeData;
}

std::unique_ptr<QMimeData> ClipboardService::imageMimeData(const QImage &image) const
{
    if (image.isNull()) {
        return nullptr;
    }

    QByteArray pngBytes;
    QBuffer buffer(&pngBytes);
    if (!buffer.open(QIODevice::WriteOnly) || !image.save(&buffer, "PNG") || pngBytes.isEmpty()) {
        return nullptr;
    }

    auto mimeData = std::make_unique<QMimeData>();
    mimeData->setData(QStringLiteral("image/png"), pngBytes);
    mimeData->setImageData(QVariant::fromValue(image));
    return mimeData;
}

bool ClipboardService::publishFilePaths(const QStringList &paths) const
{
    return publish(fileMimeData(paths));
}

bool ClipboardService::publishImage(const QImage &image) const
{
    return publish(imageMimeData(image));
}

bool ClipboardService::publish(std::unique_ptr<QMimeData> mimeData) const
{
    if (m_clipboard == nullptr || mimeData == nullptr) {
        return false;
    }
    m_clipboard->setMimeData(mimeData.release());
    return true;
}

} // namespace Astrea::Explorer::Native::Services
