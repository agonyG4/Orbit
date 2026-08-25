#pragma once

#include <QUrl>

#include "models/directory_model.h"

namespace Astrea::Explorer::Native::Backend {

class PreviewController final
{
public:
    explicit PreviewController(DirectoryModel *model);

    QUrl previewUrl(const DirectoryEntry &entry, bool remoteDirectoryActive) const;
    void beginGeneration(quint64 generation, bool remoteDirectoryActive);
    bool applyPreview(
        const QString &filePath,
        const QUrl &previewUrl,
        quint64 generation);
    quint64 generation() const;

private:
    static bool isPreviewablePath(const QString &path, bool isDirectory);

    DirectoryModel *m_model = nullptr;
    quint64 m_generation = 0;
    bool m_remoteDirectoryActive = false;
};

} // namespace Astrea::Explorer::Native::Backend
