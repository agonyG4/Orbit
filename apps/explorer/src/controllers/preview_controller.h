#pragma once

#include <QDateTime>
#include <QHash>
#include <QJsonObject>
#include <QObject>
#include <QSet>
#include <QTimer>
#include <QUrl>
#include <QVector>

#include "backend/rust_backend_client.h"
#include "models/directory_model.h"

namespace Astrea::Explorer::Native::Backend {

class PreviewController final : public QObject
{
    Q_OBJECT

public:
    explicit PreviewController(
        IRustBackendClient *client,
        DirectoryModel *model,
        QObject *parent = nullptr);

    QUrl previewUrl(const DirectoryEntry &entry, bool remoteDirectoryActive) const;
    void beginGeneration(quint64 generation, bool remoteDirectoryActive);
    void setEnabled(bool enabled);
    void requestVisibleRange(int firstIndex, int lastIndex, int physicalTarget);
    void requestSelectedPreview(const QString &filePath, int physicalTarget);
    bool applyPreview(
        const QString &filePath,
        const QUrl &previewUrl,
        quint64 generation);
    quint64 generation() const;

private slots:
    void dispatchQueued();
    void handleUtilityReady(
        BackendRequestId requestId,
        const UtilityResult &result);
    void handleBackendFailure(const BackendError &error);

private:
    struct Intent
    {
        QString path;
        int target = 128;
    };

    struct InFlightBatch
    {
        quint64 generation = 0;
        int target = 128;
        QVector<Intent> intents;
    };

    struct AppliedPreview
    {
        QString sourceVersion;
        int target = 0;
    };

    static bool isEligible(const DirectoryEntry &entry);
    static bool isDirectImagePath(const QString &path);
    static QString sourceVersion(const DirectoryEntry &entry);
    static int tierPixels(const QString &tier, int fallback);
    bool isInFlight(const QString &path) const;
    bool isSuppressed(const DirectoryEntry &entry, int target) const;
    void replaceViewportQueue(int firstIndex, int lastIndex, int target);
    void scheduleDispatch(int delayMs = 50);
    void clearQueuedWork();
    void dispatchBatch();
    void finishRequest(BackendRequestId requestId);
    void applyBatchItem(
        const InFlightBatch &request,
        const QJsonObject &item);

    IRustBackendClient *m_client = nullptr;
    DirectoryModel *m_model = nullptr;
    QTimer m_dispatchTimer;
    QHash<BackendRequestId, InFlightBatch> m_inFlight;
    QVector<Intent> m_viewportQueue;
    QSet<QString> m_viewportPaths;
    Intent m_selectedIntent;
    bool m_hasSelectedIntent = false;
    QHash<QString, AppliedPreview> m_appliedPreviews;
    QHash<QString, QString> m_failedSources;
    QHash<QString, QDateTime> m_deferredUntil;
    quint64 m_generation = 0;
    bool m_remoteDirectoryActive = false;
    bool m_enabled = true;
};

} // namespace Astrea::Explorer::Native::Backend
