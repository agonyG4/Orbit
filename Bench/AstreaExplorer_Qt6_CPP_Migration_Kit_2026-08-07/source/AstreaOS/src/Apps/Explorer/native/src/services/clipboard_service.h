#pragma once

#include <memory>

#include <QImage>
#include <QMimeData>
#include <QStringList>

class QClipboard;

namespace Astrea::Explorer::Native::Services {

class ClipboardService final
{
public:
    explicit ClipboardService(QClipboard *clipboard = nullptr);

    std::unique_ptr<QMimeData> fileMimeData(const QStringList &paths) const;
    std::unique_ptr<QMimeData> imageMimeData(const QImage &image) const;

    bool publishFilePaths(const QStringList &paths) const;
    bool publishImage(const QImage &image) const;

private:
    bool publish(std::unique_ptr<QMimeData> mimeData) const;

    QClipboard *m_clipboard = nullptr;
};

} // namespace Astrea::Explorer::Native::Services
