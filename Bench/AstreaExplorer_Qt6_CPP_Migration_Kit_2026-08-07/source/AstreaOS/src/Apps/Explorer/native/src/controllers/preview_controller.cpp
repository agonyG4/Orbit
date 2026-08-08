#include "controllers/preview_controller.h"

#include <QtGlobal>

namespace Astrea::Explorer::Native::Backend {

PreviewController::PreviewController(DirectoryModel *model)
    : m_model(model)
{
    Q_ASSERT(m_model != nullptr);
}

QUrl PreviewController::previewUrl(
    const DirectoryEntry &entry,
    bool remoteDirectoryActive) const
{
    if (remoteDirectoryActive || entry.fileRemote || entry.fileIsDir) {
        return {};
    }
    if (!entry.filePreviewUrl.isEmpty()) {
        return entry.filePreviewUrl;
    }
    if (!isPreviewablePath(entry.filePath, entry.fileIsDir)) {
        return {};
    }
    return entry.fileUrl.isEmpty() ? QUrl::fromLocalFile(entry.filePath) : entry.fileUrl;
}

void PreviewController::beginGeneration(
    quint64 generation,
    bool remoteDirectoryActive)
{
    m_remoteDirectoryActive = remoteDirectoryActive;
    m_generation = generation;
}

bool PreviewController::applyPreview(
    const QString &filePath,
    const QUrl &previewUrlToApply,
    quint64 generationToApply)
{
    if (m_remoteDirectoryActive || generationToApply != m_generation) {
        return false;
    }
    return m_model->updatePreview(filePath, previewUrlToApply, generationToApply);
}

quint64 PreviewController::generation() const
{
    return m_generation;
}

bool PreviewController::isPreviewablePath(const QString &path, bool isDirectory)
{
    if (isDirectory) {
        return false;
    }
    const QString lowerPath = path.toLower();
    return lowerPath.endsWith(QStringLiteral(".jpg"))
        || lowerPath.endsWith(QStringLiteral(".jpeg"))
        || lowerPath.endsWith(QStringLiteral(".png"))
        || lowerPath.endsWith(QStringLiteral(".gif"))
        || lowerPath.endsWith(QStringLiteral(".bmp"))
        || lowerPath.endsWith(QStringLiteral(".webp"))
        || lowerPath.endsWith(QStringLiteral(".svg"))
        || lowerPath.endsWith(QStringLiteral(".avif"))
        || lowerPath.endsWith(QStringLiteral(".heic"))
        || lowerPath.endsWith(QStringLiteral(".heif"))
        || lowerPath.endsWith(QStringLiteral(".tiff"))
        || lowerPath.endsWith(QStringLiteral(".tif"));
}

} // namespace Astrea::Explorer::Native::Backend
