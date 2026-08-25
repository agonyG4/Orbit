#pragma once

#include <QQuickImageProvider>

namespace Astrea::Explorer::Native::Services {
class IconThemeService;
}

namespace Astrea::Explorer::Native::Runtime {

class AstreaIconImageProvider final : public QQuickImageProvider
{
public:
    explicit AstreaIconImageProvider(Services::IconThemeService *service);

    QImage requestImage(
        const QString &id,
        QSize *size,
        const QSize &requestedSize) override;

private:
    Services::IconThemeService *m_service = nullptr;
};

} // namespace Astrea::Explorer::Native::Runtime
