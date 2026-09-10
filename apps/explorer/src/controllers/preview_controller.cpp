#include "controllers/preview_controller.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>
#include <QTimer>
#include <QtGlobal>

#include <limits>

namespace Astrea::Explorer::Native::Backend {

PreviewController::PreviewController(
    IRustBackendClient *client,
    DirectoryModel *model,
    QObject *parent)
    : QObject(parent)
    , m_client(client)
    , m_model(model)
{
    Q_ASSERT(m_client != nullptr);
    Q_ASSERT(m_model != nullptr);

    m_dispatchTimer.setSingleShot(true);
    connect(
        &m_dispatchTimer,
        &QTimer::timeout,
        this,
        &PreviewController::dispatchQueued);
    m_deferredRetryTimer.setSingleShot(true);
    connect(
        &m_deferredRetryTimer,
        &QTimer::timeout,
        this,
        &PreviewController::handleDeferredRetry);
    connect(
        m_client,
        &IRustBackendClient::utilityReady,
        this,
        &PreviewController::handleUtilityReady,
        Qt::QueuedConnection);
    connect(
        m_client,
        &IRustBackendClient::failed,
        this,
        &PreviewController::handleBackendFailure,
        Qt::QueuedConnection);
}

QUrl PreviewController::previewUrl(
    const DirectoryEntry &entry,
    bool remoteDirectoryActive) const
{
    if (remoteDirectoryActive
        || entry.fileRemote
        || entry.fileIsDir
        || entry.fileSymlinkBroken) {
        return {};
    }
    if (!entry.filePreviewUrl.isEmpty()) {
        return entry.filePreviewUrl;
    }
    if (!isDirectImagePath(entry.filePath)) {
        return {};
    }
    return entry.fileUrl.isEmpty()
        ? QUrl::fromLocalFile(entry.filePath)
        : entry.fileUrl;
}

void PreviewController::beginGeneration(
    quint64 generation,
    bool remoteDirectoryActive)
{
    m_generation = generation;
    m_remoteDirectoryActive = remoteDirectoryActive;
    m_dispatchTimer.stop();
    m_deferredRetryTimer.stop();
    clearQueuedWork();
    m_appliedPreviews.clear();
    m_failedSources.clear();
    m_unsupportedSources.clear();
    m_unavailableSources.clear();
    m_deferredPreviews.clear();
    m_hasVisibleRange = false;
}

void PreviewController::setEnabled(bool enabled)
{
    if (m_enabled == enabled) {
        return;
    }
    m_enabled = enabled;
    if (!m_enabled) {
        m_dispatchTimer.stop();
        m_deferredRetryTimer.stop();
        clearQueuedWork();
    }
}

void PreviewController::requestVisibleRange(
    int firstIndex,
    int lastIndex,
    int physicalTarget)
{
    m_hasVisibleRange = m_enabled
        && !m_remoteDirectoryActive
        && lastIndex >= firstIndex;
    if (m_hasVisibleRange) {
        m_lastVisibleFirst = firstIndex;
        m_lastVisibleLast = lastIndex;
        m_lastVisibleTarget = qMax(1, physicalTarget);
    }
    replaceViewportQueue(firstIndex, lastIndex, physicalTarget);
    armDeferredRetryTimer();
}

void PreviewController::requestSelectedPreview(
    const QString &filePath,
    int physicalTarget)
{
    if (!m_enabled || m_remoteDirectoryActive) {
        return;
    }
    if (filePath.isEmpty()) {
        m_selectedPath.clear();
        m_hasSelectedPath = false;
        m_selectedIntent = {};
        m_hasSelectedIntent = false;
        return;
    }
    m_selectedPath = filePath;
    m_hasSelectedPath = true;

    DirectoryEntry entry;
    if (!m_model->entryForPath(filePath, &entry) || !isEligible(entry)) {
        return;
    }

    const int target = qMax(1, physicalTarget);
    if (isSuppressed(entry, target)) {
        return;
    }
    const auto inFlightTarget = inFlightTargetForPath(filePath);
    if (inFlightTarget.has_value() && *inFlightTarget >= target) {
        return;
    }

    if (m_hasSelectedIntent && m_selectedIntent.path == filePath) {
        m_selectedIntent.target = qMax(m_selectedIntent.target, target);
    } else {
        m_selectedIntent = {filePath, target};
        m_hasSelectedIntent = true;
    }
    scheduleDispatch(0);
}

bool PreviewController::applyPreview(
    const QString &filePath,
    const QUrl &previewUrlToApply,
    quint64 generationToApply)
{
    if (m_remoteDirectoryActive
        || generationToApply != m_generation
        || !previewUrlToApply.isValid()) {
        return false;
    }
    return m_model->updatePreview(filePath, previewUrlToApply, generationToApply);
}

quint64 PreviewController::generation() const
{
    return m_generation;
}

bool PreviewController::isEligible(const DirectoryEntry &entry)
{
    return !entry.filePath.isEmpty()
        && !entry.fileRemote
        && !entry.fileIsDir
        && !entry.fileSymlinkBroken
        && !entry.fileMetadataLimited;
}

bool PreviewController::isDirectImagePath(const QString &path)
{
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

QString PreviewController::sourceVersion(const DirectoryEntry &entry)
{
    const qint64 modifiedSeconds = entry.fileModified.isValid()
        ? entry.fileModified.toSecsSinceEpoch()
        : 0;
    return QStringLiteral("%1:%2")
        .arg(modifiedSeconds)
        .arg(entry.fileSize);
}

int PreviewController::tierPixels(const QString &tier, int fallback)
{
    if (tier == QStringLiteral("normal")) {
        return 128;
    }
    if (tier == QStringLiteral("large")) {
        return 256;
    }
    if (tier == QStringLiteral("x-large")) {
        return 512;
    }
    if (tier == QStringLiteral("xx-large")) {
        return 1024;
    }
    return fallback;
}

std::optional<int> PreviewController::inFlightTargetForPath(const QString &path) const
{
    std::optional<int> target;
    for (auto it = m_inFlight.cbegin(); it != m_inFlight.cend(); ++it) {
        if (it.value().generation != m_generation) {
            continue;
        }
        for (const Intent &intent : it.value().intents) {
            if (intent.path == path) {
                if (!target.has_value() || it.value().target > *target) {
                    target = it.value().target;
                }
            }
        }
    }
    return target;
}

bool PreviewController::isSuppressed(
    const DirectoryEntry &entry,
    int target)
{
    const QString version = sourceVersion(entry);
    auto applied = m_appliedPreviews.constFind(entry.filePath);
    if (applied != m_appliedPreviews.cend()) {
        if (applied->sourceVersion != version) {
            m_appliedPreviews.remove(entry.filePath);
        } else if (applied->target >= target) {
            return true;
        }
    }
    auto failed = m_failedSources.constFind(entry.filePath);
    if (failed != m_failedSources.cend()) {
        if (failed.value() != version) {
            m_failedSources.remove(entry.filePath);
        } else {
            return true;
        }
    }
    auto unsupported = m_unsupportedSources.constFind(entry.filePath);
    if (unsupported != m_unsupportedSources.cend()) {
        if (unsupported.value() != version) {
            m_unsupportedSources.remove(entry.filePath);
        } else {
            return true;
        }
    }
    auto unavailable = m_unavailableSources.constFind(entry.filePath);
    if (unavailable != m_unavailableSources.cend()) {
        if (unavailable.value() != version) {
            m_unavailableSources.remove(entry.filePath);
        } else {
            return true;
        }
    }
    const auto deferred = m_deferredPreviews.constFind(entry.filePath);
    if (deferred == m_deferredPreviews.cend()) {
        return false;
    }
    if (deferred->sourceVersion != version) {
        m_deferredPreviews.remove(entry.filePath);
        return false;
    }
    return deferred->retryAfter > QDateTime::currentDateTimeUtc();
}

void PreviewController::replaceViewportQueue(
    int firstIndex,
    int lastIndex,
    int target)
{
    m_viewportQueue.clear();
    m_viewportPaths.clear();
    if (!m_enabled || m_remoteDirectoryActive || lastIndex < firstIndex) {
        return;
    }

    const QVector<QString> paths = m_model->paths();
    const int first = qBound(0, firstIndex, paths.size());
    const int last = qBound(-1, lastIndex, paths.size() - 1);
    if (first > last) {
        return;
    }

    const int normalizedTarget = qMax(1, target);
    for (int index = first; index <= last; ++index) {
        DirectoryEntry entry;
        if (!m_model->entryForPath(paths.at(index), &entry)
            || !isEligible(entry)
            || isSuppressed(entry, normalizedTarget)) {
            continue;
        }
        const auto inFlightTarget = inFlightTargetForPath(entry.filePath);
        if (inFlightTarget.has_value() && *inFlightTarget >= normalizedTarget) {
            continue;
        }
        m_viewportQueue.append({entry.filePath, normalizedTarget});
        m_viewportPaths.insert(entry.filePath);
    }
    if (!m_viewportQueue.isEmpty()) {
        scheduleDispatch();
    }
}

void PreviewController::scheduleDispatch(int delayMs)
{
    if (!m_enabled || m_remoteDirectoryActive) {
        return;
    }
    if (!m_viewportQueue.isEmpty() || m_hasSelectedIntent) {
        m_dispatchTimer.start(qMax(0, delayMs));
    }
}

void PreviewController::armDeferredRetryTimer()
{
    if (!m_enabled || m_remoteDirectoryActive) {
        m_deferredRetryTimer.stop();
        return;
    }

    const QDateTime now = QDateTime::currentDateTimeUtc();
    QDateTime earliest;
    for (auto it = m_deferredPreviews.cbegin();
         it != m_deferredPreviews.cend();
         ++it) {
        if (!it.value().retryable) {
            continue;
        }
        if (it.value().retryAfter <= now) {
            continue;
        }
        if (!earliest.isValid() || it.value().retryAfter < earliest) {
            earliest = it.value().retryAfter;
        }
    }
    if (!earliest.isValid()) {
        m_deferredRetryTimer.stop();
        return;
    }

    const qint64 delay = now.msecsTo(earliest);
    const qint64 boundedDelay = qMax<qint64>(
        1,
        qMin<qint64>(delay, std::numeric_limits<int>::max()));
    m_deferredRetryTimer.start(static_cast<int>(boundedDelay));
}

void PreviewController::clearQueuedWork()
{
    m_viewportQueue.clear();
    m_viewportPaths.clear();
    m_selectedIntent = {};
    m_hasSelectedIntent = false;
    m_selectedPath.clear();
    m_hasSelectedPath = false;
}

void PreviewController::handleDeferredRetry()
{
    if (!m_enabled || m_remoteDirectoryActive) {
        return;
    }

    const QDateTime now = QDateTime::currentDateTimeUtc();
    bool hasDueRetry = false;
    for (auto it = m_deferredPreviews.cbegin();
         it != m_deferredPreviews.cend();
         ++it) {
        if (it.value().retryable && it.value().retryAfter <= now) {
            hasDueRetry = true;
            break;
        }
    }
    if (!hasDueRetry) {
        armDeferredRetryTimer();
        return;
    }

    if (m_hasVisibleRange) {
        replaceViewportQueue(
            m_lastVisibleFirst,
            m_lastVisibleLast,
            m_lastVisibleTarget);
    }

    for (auto it = m_deferredPreviews.cbegin();
         it != m_deferredPreviews.cend();
         ++it) {
        const DeferredPreviewState &deferred = it.value();
        if (!deferred.retryable
            || !deferred.selectedPriority
            || !m_hasSelectedPath
            || deferred.intent.path != m_selectedPath
            || deferred.retryAfter > now) {
            continue;
        }
        DirectoryEntry entry;
        if (!m_model->entryForPath(deferred.intent.path, &entry)
            || !isEligible(entry)
            || sourceVersion(entry) != deferred.sourceVersion
            || isSuppressed(entry, deferred.intent.target)) {
            continue;
        }
        const auto inFlightTarget = inFlightTargetForPath(deferred.intent.path);
        if (inFlightTarget.has_value()
            && *inFlightTarget >= deferred.intent.target) {
            continue;
        }
        if (m_hasSelectedIntent
            && m_selectedIntent.path != deferred.intent.path) {
            m_selectedIntent = deferred.intent;
        } else if (!m_hasSelectedIntent) {
            m_selectedIntent = deferred.intent;
        } else {
            m_selectedIntent.target = qMax(
                m_selectedIntent.target,
                deferred.intent.target);
        }
        m_hasSelectedIntent = true;
    }
    if (!m_viewportQueue.isEmpty() || m_hasSelectedIntent) {
        scheduleDispatch(0);
    }
    armDeferredRetryTimer();
}

void PreviewController::dispatchQueued()
{
    dispatchBatch();
}

void PreviewController::dispatchBatch()
{
    if (!m_enabled
        || m_remoteDirectoryActive
        || !m_inFlight.isEmpty()
        || m_client == nullptr) {
        return;
    }

    QVector<Intent> intents;
    int target = 128;
    bool selectedPriority = false;
    if (m_hasSelectedIntent) {
        const auto inFlightTarget = inFlightTargetForPath(m_selectedIntent.path);
        if (!inFlightTarget.has_value()
            || *inFlightTarget < m_selectedIntent.target) {
            intents.append(m_selectedIntent);
            target = m_selectedIntent.target;
            selectedPriority = true;
        }
        m_hasSelectedIntent = false;
        m_selectedIntent = {};
    } else {
        while (!m_viewportQueue.isEmpty()) {
            const Intent intent = m_viewportQueue.constFirst();
            DirectoryEntry entry;
            if (!m_model->entryForPath(intent.path, &entry)
                || !isEligible(entry)
                || isSuppressed(entry, intent.target)) {
                m_viewportPaths.remove(intent.path);
                m_viewportQueue.removeFirst();
                continue;
            }
            const auto inFlightTarget = inFlightTargetForPath(intent.path);
            if (inFlightTarget.has_value()
                && *inFlightTarget >= intent.target) {
                m_viewportPaths.remove(intent.path);
                m_viewportQueue.removeFirst();
                continue;
            }
            break;
        }
        if (m_viewportQueue.isEmpty()) {
            m_viewportPaths.clear();
            return;
        }

        target = m_viewportQueue.constFirst().target;
        int argumentBytes = QString::number(target).toUtf8().size() + 1;
        while (!m_viewportQueue.isEmpty()
               && intents.size() < 32
               && m_viewportQueue.constFirst().target == target) {
            const Intent intent = m_viewportQueue.constFirst();
            const int pathBytes = intent.path.toUtf8().size() + 1;
            if (!intents.isEmpty() && argumentBytes + pathBytes > 262144) {
                break;
            }
            m_viewportQueue.removeFirst();
            m_viewportPaths.remove(intent.path);
            intents.append(intent);
            argumentBytes += pathBytes;
        }
    }

    if (intents.isEmpty()) {
        if (!m_viewportQueue.isEmpty()) {
            scheduleDispatch(0);
        }
        return;
    }

    UtilityRequest utilityRequest;
    utilityRequest.operation = QStringLiteral("thumbnail-batch");
    utilityRequest.arguments.append(QString::number(target));
    for (const Intent &intent : intents) {
        utilityRequest.arguments.append(intent.path);
    }

    const BackendRequestId requestId = m_client->utility(utilityRequest);
    m_inFlight.insert(
        requestId,
        InFlightBatch {m_generation, target, intents, selectedPriority});
}

void PreviewController::finishRequest(BackendRequestId requestId)
{
    if (m_inFlight.remove(requestId) == 0) {
        return;
    }
    if (m_enabled && !m_remoteDirectoryActive
        && (!m_viewportQueue.isEmpty() || m_hasSelectedIntent)) {
        scheduleDispatch(0);
    }
}

void PreviewController::handleUtilityReady(
    BackendRequestId requestId,
    const UtilityResult &result)
{
    const auto request = m_inFlight.constFind(requestId);
    if (request == m_inFlight.cend()) {
        return;
    }
    const InFlightBatch batch = request.value();
    m_inFlight.remove(requestId);

    if (result.operation == QStringLiteral("thumbnail-batch")) {
        const QJsonArray items = result.data.value(QStringLiteral("items")).toArray();
        for (const QJsonValue &value : items) {
            if (value.isObject()) {
                applyBatchItem(batch, value.toObject());
            }
        }
    }

    if (m_enabled && !m_remoteDirectoryActive
        && (!m_viewportQueue.isEmpty() || m_hasSelectedIntent)) {
        scheduleDispatch(0);
    }
}

void PreviewController::handleBackendFailure(const BackendError &error)
{
    finishRequest(error.requestId);
}

void PreviewController::applyBatchItem(
    const InFlightBatch &request,
    const QJsonObject &item)
{
    const QString filePath = item.value(QStringLiteral("filePath")).toString();
    if (filePath.isEmpty()) {
        return;
    }

    bool requested = false;
    for (const Intent &intent : request.intents) {
        if (intent.path == filePath) {
            requested = true;
            break;
        }
    }
    if (!requested || request.generation != m_generation
        || m_remoteDirectoryActive || !m_enabled) {
        return;
    }

    DirectoryEntry entry;
    if (!m_model->entryForPath(filePath, &entry)) {
        return;
    }
    const QString currentVersion = sourceVersion(entry);
    if (item.value(QStringLiteral("sourceVersion")).toString() != currentVersion) {
        return;
    }

    const QString status = item.value(QStringLiteral("status")).toString();
    if (status == QStringLiteral("deferred")) {
        Intent deferredIntent;
        for (const Intent &intent : request.intents) {
            if (intent.path == filePath) {
                deferredIntent = intent;
                break;
            }
        }
        if (!item.value(QStringLiteral("retryable")).toBool(false)) {
            m_deferredPreviews.remove(filePath);
            m_unavailableSources.insert(filePath, currentVersion);
            return;
        }
        m_deferredPreviews.insert(
            filePath,
            DeferredPreviewState {
                currentVersion,
                QDateTime::currentDateTimeUtc().addMSecs(
                    qMax(1, item.value(QStringLiteral("retryAfterMs")).toInt())),
                deferredIntent,
                true,
                request.selectedPriority
            });
        armDeferredRetryTimer();
        return;
    }
    if (status == QStringLiteral("unavailable")) {
        m_deferredPreviews.remove(filePath);
        m_unavailableSources.insert(filePath, currentVersion);
        return;
    }
    if (status == QStringLiteral("unsupported")) {
        m_deferredPreviews.remove(filePath);
        m_unsupportedSources.insert(filePath, currentVersion);
        armDeferredRetryTimer();
        return;
    }
    if (status == QStringLiteral("failed")) {
        m_deferredPreviews.remove(filePath);
        m_failedSources.insert(filePath, currentVersion);
        armDeferredRetryTimer();
        return;
    }
    if (status != QStringLiteral("ready")
        && status != QStringLiteral("generated")
        && status != QStringLiteral("cached")
        && status != QStringLiteral("direct")) {
        return;
    }

    const QUrl url(item.value(QStringLiteral("previewUrl")).toString());
    if (!url.isValid() || !url.isLocalFile()) {
        return;
    }
    if (!applyPreview(filePath, url, request.generation)) {
        return;
    }

    m_deferredPreviews.remove(filePath);
    m_unsupportedSources.remove(filePath);
    m_failedSources.remove(filePath);
    m_unavailableSources.remove(filePath);
    m_appliedPreviews.insert(
        filePath,
        AppliedPreview {
            currentVersion,
            tierPixels(
                item.value(QStringLiteral("cacheTier")).toString(),
                request.target),
        });
    armDeferredRetryTimer();
}

} // namespace Astrea::Explorer::Native::Backend
