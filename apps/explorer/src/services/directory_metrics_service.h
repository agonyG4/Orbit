#pragma once

#include <QObject>

#include "backend/rust_backend_client.h"

namespace Astrea::Explorer::Native::Services {

class DirectoryMetricsService final : public QObject
{
    Q_OBJECT

public:
    explicit DirectoryMetricsService(
        Backend::IRustBackendClient *client,
        QObject *parent = nullptr);

    Backend::BackendRequestId start(const QStringList &paths);
    void cancel(Backend::BackendRequestId requestId);
    Backend::BackendRequestId activeRequest() const;

signals:
    void progress(
        Astrea::Explorer::Native::Backend::BackendRequestId requestId,
        const Astrea::Explorer::Native::Backend::DirectoryMetricsProgress &progress);
    void finished(
        Astrea::Explorer::Native::Backend::BackendRequestId requestId,
        const Astrea::Explorer::Native::Backend::DirectoryMetricsResult &result);
    void failed(const Astrea::Explorer::Native::Backend::BackendError &error);
    void superseded(Astrea::Explorer::Native::Backend::BackendRequestId requestId);

private slots:
    void handleProgress(
        Backend::BackendRequestId requestId,
        const Backend::DirectoryMetricsProgress &progress);
    void handleFinished(
        Backend::BackendRequestId requestId,
        const Backend::DirectoryMetricsResult &result);
    void handleFailure(const Backend::BackendError &error);

private:
    Backend::IRustBackendClient *m_client = nullptr;
    Backend::BackendRequestId m_activeRequest = 0;
};

} // namespace Astrea::Explorer::Native::Services
