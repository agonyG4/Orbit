#pragma once

#include <QByteArray>
#include <QObject>
#include <QStringList>

#include "backend/backend_types.h"

namespace Astrea::Explorer::Native::Backend {

class BackendTransport : public QObject
{
    Q_OBJECT

public:
    explicit BackendTransport(QObject *parent = nullptr);
    ~BackendTransport() override;

    virtual BackendRequestId start(const QStringList &arguments) = 0;
    virtual void cancel(BackendRequestId requestId) = 0;

signals:
    void completed(
        Astrea::Explorer::Native::Backend::BackendRequestId requestId,
        const QByteArray &payload);
    void streamed(
        Astrea::Explorer::Native::Backend::BackendRequestId requestId,
        const QByteArray &payload);
    void failed(
        Astrea::Explorer::Native::Backend::BackendRequestId requestId,
        const Astrea::Explorer::Native::Backend::BackendTransportError &error);

protected:
    BackendRequestId allocateRequestId();
    void emitCompleted(BackendRequestId requestId, const QByteArray &payload);
    void emitStreamed(BackendRequestId requestId, const QByteArray &payload);
    void emitFailed(
        BackendRequestId requestId,
        const BackendTransportError &error);

private:
    BackendRequestId m_nextRequestId = 1;
};

} // namespace Astrea::Explorer::Native::Backend
