#include "controllers/file_operations_controller.h"

#include <algorithm>
#include <QDir>
#include <QFileInfo>

#include "services/clipboard_service.h"
#include "services/file_operation_service.h"

namespace Astrea::Explorer::Native::Backend {

FileOperationsController::FileOperationsController(
    Services::FileOperationService *service,
    Services::ClipboardService *clipboardService,
    QObject *parent)
    : QObject(parent)
    , m_service(service)
    , m_clipboardService(clipboardService)
{
    Q_ASSERT(m_service != nullptr);
    connect(
        m_service,
        &Services::FileOperationService::progress,
        this,
        &FileOperationsController::handleProgress);
    connect(
        m_service,
        &Services::FileOperationService::finished,
        this,
        &FileOperationsController::handleFinished);
    connect(
        m_service,
        &Services::FileOperationService::failed,
        this,
        &FileOperationsController::handleFailure);
}

QStringList FileOperationsController::clipboardFiles() const
{
    return m_clipboardFiles;
}

QString FileOperationsController::clipboardMode() const
{
    return m_clipboardMode;
}

bool FileOperationsController::running() const
{
    return m_running;
}

double FileOperationsController::operationProgress() const
{
    return m_operationProgress;
}

int FileOperationsController::operationPercent() const
{
    return m_operationPercent;
}

QString FileOperationsController::operationFileName() const
{
    return m_operationFileName;
}

QString FileOperationsController::operationStatus() const
{
    return m_operationStatus;
}

QString FileOperationsController::operationError() const
{
    return m_operationError;
}

QString FileOperationsController::operationDestination() const
{
    return m_operationDestination;
}

int FileOperationsController::operationDoneCount() const
{
    return m_operationDoneCount;
}

int FileOperationsController::operationTotalCount() const
{
    return m_operationTotalCount;
}

QString FileOperationsController::operationMode() const
{
    return m_operationMode;
}

QString FileOperationsController::operationState() const
{
    return m_operationState;
}

QVariantList FileOperationsController::operationItems() const
{
    return m_operationItems;
}

bool FileOperationsController::pasteConflictVisible() const
{
    return m_pasteConflictVisible;
}

QVariantList FileOperationsController::pasteConflictItems() const
{
    return m_pasteConflictItems;
}

QString FileOperationsController::pendingPasteRename() const
{
    return m_pendingPasteRename;
}

BackendRequestId FileOperationsController::operationRequestId() const
{
    return m_operationRequestId;
}

void FileOperationsController::setSelection(const QStringList &paths)
{
    m_selection = paths;
}

void FileOperationsController::copySelection()
{
    if (m_selection.isEmpty()) {
        return;
    }
    m_clipboardFiles = m_selection;
    m_clipboardMode = QStringLiteral("copy");
    if (m_clipboardService != nullptr) {
        m_clipboardService->publishFilePaths(m_clipboardFiles);
    }
    emit clipboardChanged();
}

void FileOperationsController::cutSelection()
{
    if (m_selection.isEmpty()) {
        return;
    }

    QStringList sortedSelection = m_selection;
    QStringList sortedClipboard = m_clipboardFiles;
    std::sort(sortedSelection.begin(), sortedSelection.end());
    std::sort(sortedClipboard.begin(), sortedClipboard.end());
    if (m_clipboardMode == QStringLiteral("cut") && sortedSelection == sortedClipboard) {
        m_clipboardFiles.clear();
        m_clipboardMode.clear();
        if (m_clipboardService != nullptr) {
            m_clipboardService->clear();
        }
        emit clipboardChanged();
        return;
    }

    m_clipboardFiles = m_selection;
    m_clipboardMode = QStringLiteral("cut");
    if (m_clipboardService != nullptr) {
        m_clipboardService->publishFilePaths(m_clipboardFiles);
    }
    emit clipboardChanged();
}

BackendRequestId FileOperationsController::pasteFiles(
    const QString &destination,
    const QString &conflictPolicy)
{
    if (m_clipboardFiles.isEmpty() && m_clipboardService != nullptr) {
        const QStringList systemFiles = m_clipboardService->filePaths();
        if (!systemFiles.isEmpty()) {
            m_clipboardFiles = systemFiles;
            m_clipboardMode = QStringLiteral("copy");
            emit clipboardChanged();
        } else {
            QString error;
            const QString imagePath = m_clipboardService->pasteImage(destination, &error);
            if (!imagePath.isEmpty()) {
                emit imagePasted(imagePath);
            }
            return 0;
        }
    }
    if (m_clipboardFiles.isEmpty() || m_running) {
        return 0;
    }

    if (!m_conflictResolutionInProgress
        && showConflictPrompt(m_clipboardFiles, destination, m_clipboardMode)) {
        return 0;
    }
    m_conflictResolutionInProgress = false;
    m_pendingTransferSources.clear();

    m_operationDestination = destination;
    m_operationMode = m_clipboardMode == QStringLiteral("cut")
        ? QStringLiteral("move")
        : QStringLiteral("copy");
    m_operationError.clear();
    m_operationStatus = m_operationMode == QStringLiteral("move")
        ? QStringLiteral("Moving")
        : QStringLiteral("Copying");
    m_operationDoneCount = 0;
    m_operationTotalCount = m_clipboardFiles.size();
    m_operationPercent = 0;
    m_operationProgress = 0.0;
    m_operationState = QStringLiteral("running");
    m_operationItems.clear();
    const BackendRequestId requestId = m_service->start(
        makePasteRequest(destination, conflictPolicy));
    m_operationRequestId = requestId;
    setRunning(true);
    return requestId;
}

BackendRequestId FileOperationsController::transferFiles(
    const QStringList &sources,
    const QString &destination,
    const QString &mode,
    const QString &conflictPolicy)
{
    if (sources.isEmpty() || destination.isEmpty() || m_running) {
        return 0;
    }

    if (!m_conflictResolutionInProgress
        && showConflictPrompt(sources, destination, mode)) {
        return 0;
    }
    m_conflictResolutionInProgress = false;
    m_pendingTransferSources.clear();
    m_pasteConflictVisible = false;
    m_pasteConflictItems.clear();
    m_pendingPasteDestination.clear();
    m_pendingPasteRename.clear();
    emit pasteConflictChanged();

    m_operationDestination = destination;
    m_operationMode = mode == QStringLiteral("move")
        ? QStringLiteral("move")
        : QStringLiteral("copy");
    m_operationError.clear();
    m_operationStatus = m_operationMode == QStringLiteral("move")
        ? QStringLiteral("Moving")
        : QStringLiteral("Copying");
    m_operationDoneCount = 0;
    m_operationTotalCount = sources.size();
    m_operationPercent = 0;
    m_operationProgress = 0.0;
    m_operationFileName.clear();
    m_operationState = QStringLiteral("running");
    m_operationItems.clear();
    const BackendRequestId requestId = m_service->start(
        makeTransferRequest(sources, destination, m_operationMode, conflictPolicy));
    m_operationRequestId = requestId;
    setRunning(true);
    return requestId;
}

void FileOperationsController::cancelOperation()
{
    if (!m_running || m_operationRequestId == 0) {
        return;
    }
    m_service->cancel(m_operationRequestId);
    resetOperationState();
}

bool FileOperationsController::isCutPending(const QString &name) const
{
    if (m_clipboardMode != QStringLiteral("cut")) {
        return false;
    }
    return std::any_of(
        m_clipboardFiles.cbegin(),
        m_clipboardFiles.cend(),
        [&name](const QString &path) { return QFileInfo(path).fileName() == name; });
}

bool FileOperationsController::isCutPathPending(const QString &path) const
{
    if (m_clipboardMode != QStringLiteral("cut")) {
        return false;
    }
    const QString normalizedPath = QDir::cleanPath(path);
    return std::any_of(
        m_clipboardFiles.cbegin(),
        m_clipboardFiles.cend(),
        [&normalizedPath](const QString &candidate) {
            return QDir::cleanPath(candidate) == normalizedPath;
        });
}

void FileOperationsController::resolvePasteConflict(const QString &conflictPolicy)
{
    if (!m_pasteConflictVisible) {
        return;
    }
    const QStringList pendingSources = m_pendingTransferSources;
    const QString pendingDestination = m_pendingPasteDestination;
    const QString pendingMode = m_pendingTransferMode;
    m_pasteConflictVisible = false;
    m_pasteConflictItems.clear();
    emit pasteConflictChanged();
    m_conflictResolutionInProgress = true;
    if (!pendingSources.isEmpty()) {
        transferFiles(pendingSources, pendingDestination, pendingMode, conflictPolicy);
    } else {
        pasteFiles(pendingDestination, conflictPolicy);
    }
}

void FileOperationsController::renamePasteConflict(const QString &name)
{
    setPendingPasteRename(name);
    resolvePasteConflict(QStringLiteral("rename"));
}

void FileOperationsController::cancelPasteConflict()
{
    if (!m_pasteConflictVisible) {
        return;
    }
    m_pasteConflictVisible = false;
    m_pasteConflictItems.clear();
    m_pendingTransferSources.clear();
    m_pendingTransferMode.clear();
    m_pendingPasteDestination.clear();
    emit pasteConflictChanged();
}

void FileOperationsController::setPendingPasteRename(const QString &name)
{
    if (m_pendingPasteRename == name) {
        return;
    }
    m_pendingPasteRename = name;
    emit pasteConflictChanged();
}

void FileOperationsController::handleProgress(
    BackendRequestId requestId,
    const FileOperationProgress &progress)
{
    if (requestId != m_operationRequestId) {
        return;
    }
    m_operationDoneCount = progress.doneCount;
    m_operationTotalCount = progress.totalCount;
    m_operationPercent = progress.percent;
    m_operationProgress = progress.percent / 100.0;
    m_operationFileName = progress.fileName;
    emit operationStateChanged();
}

void FileOperationsController::handleFinished(
    BackendRequestId requestId,
    const FileOperationResult &result)
{
    if (requestId != m_operationRequestId) {
        return;
    }
    m_operationDoneCount = result.doneCount;
    m_operationTotalCount = result.totalCount;
    m_operationPercent = result.percent;
    m_operationProgress = result.percent / 100.0;
    m_operationError = result.errorMessage;
    m_operationState = result.state.isEmpty()
        ? (result.ok ? QStringLiteral("success") : QStringLiteral("failed"))
        : result.state;
    m_operationItems.clear();
    for (const FileOperationItemResult &item : result.items) {
        QVariantMap itemMap;
        itemMap.insert(QStringLiteral("source"), item.source);
        itemMap.insert(QStringLiteral("target"), item.target);
        itemMap.insert(QStringLiteral("status"), item.status);
        itemMap.insert(QStringLiteral("errorCode"), item.errorCode);
        itemMap.insert(QStringLiteral("errorMessage"), item.errorMessage);
        m_operationItems.append(itemMap);
    }
    if (m_operationMode == QStringLiteral("move")
        && m_clipboardMode == QStringLiteral("cut")) {
        const QStringList clipboardBefore = m_clipboardFiles;
        if (result.items.isEmpty()) {
            if (result.ok) {
                m_clipboardFiles.clear();
            }
        } else {
            for (const FileOperationItemResult &item : result.items) {
                if (item.status != QStringLiteral("moved")) {
                    continue;
                }
                const QString normalizedSource = QDir::cleanPath(item.source);
                m_clipboardFiles.removeIf([&normalizedSource](const QString &path) {
                    return QDir::cleanPath(path) == normalizedSource;
                });
            }
        }
        if (m_clipboardFiles != clipboardBefore) {
            if (m_clipboardFiles.isEmpty()) {
                m_clipboardMode.clear();
                if (m_clipboardService != nullptr) {
                    m_clipboardService->clear();
                }
            } else if (m_clipboardService != nullptr) {
                m_clipboardService->publishFilePaths(m_clipboardFiles);
            }
            emit clipboardChanged();
        }
    }
    publishTerminalState();
    emit operationFinished(result);
}

void FileOperationsController::handleFailure(const BackendError &error)
{
    if (error.requestId != m_operationRequestId) {
        return;
    }
    m_operationError = error.message;
    m_operationStatus.clear();
    m_operationState = QStringLiteral("failed");
    m_operationItems.clear();
    publishTerminalState();
}

void FileOperationsController::setRunning(bool runningValue)
{
    if (m_running == runningValue) {
        return;
    }
    m_running = runningValue;
    emit operationStateChanged();
}

void FileOperationsController::publishTerminalState()
{
    m_running = false;
    emit operationStateChanged();
}

void FileOperationsController::resetOperationState()
{
    m_operationRequestId = 0;
    m_operationStatus = QStringLiteral("Cancelled");
    m_operationError.clear();
    m_operationState = QStringLiteral("cancelled");
    publishTerminalState();
}

FileOperationRequest FileOperationsController::makePasteRequest(
    const QString &destination,
    const QString &conflictPolicy) const
{
    FileOperationRequest request;
    request.mode = m_clipboardMode == QStringLiteral("cut")
        ? QStringLiteral("move")
        : QStringLiteral("copy");
    request.destination = destination;
    request.conflictPolicy = conflictPolicy;
    request.progressMode = QStringLiteral("items");
    request.sources = m_clipboardFiles;
    if (!m_pendingPasteRename.isEmpty()) {
        request.rename = m_pendingPasteRename;
    }
    return request;
}

FileOperationRequest FileOperationsController::makeTransferRequest(
    const QStringList &sources,
    const QString &destination,
    const QString &mode,
    const QString &conflictPolicy) const
{
    FileOperationRequest request;
    request.mode = mode == QStringLiteral("move") ? QStringLiteral("move") : QStringLiteral("copy");
    request.destination = destination;
    request.conflictPolicy = conflictPolicy;
    request.progressMode = QStringLiteral("items");
    request.sources = sources;
    return request;
}

QVariantList FileOperationsController::findConflicts(
    const QStringList &sources,
    const QString &destination) const
{
    QVariantList conflicts;
    const QDir targetDirectory(destination);
    if (!targetDirectory.exists()) {
        return conflicts;
    }
    for (const QString &source : sources) {
        const QString name = QFileInfo(source).fileName();
        if (name.isEmpty()) {
            continue;
        }
        if (QFileInfo::exists(targetDirectory.filePath(name))) {
            conflicts.append(source);
        }
    }
    return conflicts;
}

bool FileOperationsController::showConflictPrompt(
    const QStringList &sources,
    const QString &destination,
    const QString &mode)
{
    const QVariantList conflicts = findConflicts(sources, destination);
    if (conflicts.isEmpty()) {
        return false;
    }
    m_pasteConflictVisible = true;
    m_pasteConflictItems = conflicts;
    m_pendingPasteDestination = destination;
    m_pendingTransferSources = sources;
    m_pendingTransferMode = mode == QStringLiteral("move")
        ? QStringLiteral("move")
        : QStringLiteral("copy");
    m_pendingPasteRename.clear();
    emit pasteConflictChanged();
    return true;
}

} // namespace Astrea::Explorer::Native::Backend
