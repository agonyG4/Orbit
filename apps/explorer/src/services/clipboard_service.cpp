#include "services/clipboard_service.h"

#include <QBuffer>
#include <QClipboard>
#include <QDir>
#include <QFileInfo>
#include <QSaveFile>
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

bool ClipboardService::clear() const
{
    if (m_clipboard == nullptr) {
        return false;
    }
    m_clipboard->clear(QClipboard::Clipboard);
    return true;
}

QStringList ClipboardService::filePaths() const
{
    if (m_clipboard == nullptr || m_clipboard->mimeData() == nullptr) {
        return {};
    }
    QStringList paths;
    for (const QUrl &url : m_clipboard->mimeData()->urls()) {
        if (url.isLocalFile()) {
            paths.append(url.toLocalFile());
        }
    }
    return paths;
}

QImage ClipboardService::image() const
{
    if (m_clipboard == nullptr || m_clipboard->mimeData() == nullptr) {
        return {};
    }
    const QMimeData *mimeData = m_clipboard->mimeData();
    if (mimeData->imageData().canConvert<QImage>()) {
        return qvariant_cast<QImage>(mimeData->imageData());
    }
    for (const QString &format : mimeData->formats()) {
        if (format.startsWith(QStringLiteral("image/"))) {
            const QImage decoded = QImage::fromData(mimeData->data(format));
            if (!decoded.isNull()) {
                return decoded;
            }
        }
    }
    return {};
}

QString ClipboardService::pasteImage(const QString &directory, QString *error) const
{
    const QImage clipboardImage = image();
    if (clipboardImage.isNull()) {
        if (error != nullptr) {
            *error = QStringLiteral("clipboard does not contain an image");
        }
        return {};
    }
    const QDir destination(directory);
    if (!destination.exists() && !QDir().mkpath(directory)) {
        if (error != nullptr) {
            *error = QStringLiteral("could not create image destination");
        }
        return {};
    }
    QString target = destination.filePath(QStringLiteral("pasted-image.png"));
    for (int index = 2; QFileInfo::exists(target); ++index) {
        target = destination.filePath(QStringLiteral("pasted-image %1.png").arg(index));
    }
    QSaveFile output(target);
    if (!output.open(QIODevice::WriteOnly) || !clipboardImage.save(&output, "PNG")
        || !output.commit()) {
        if (error != nullptr) {
            *error = QStringLiteral("could not write clipboard image");
        }
        return {};
    }
    return target;
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
