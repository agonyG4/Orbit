#include "controllers/file_operations_controller.h"

#include <algorithm>
#include <QFileInfo>

#include "services/file_operation_service.h"

namespace Astrea::Explorer::Native::Backend {

FileOperationsController::FileOperationsController(
    Services::FileOperationService *service,
    QObject *parent)
    : QObject(parent)
    , m_service(service)
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
    m_clipboardFiles = m_selection;
    m_clipboardMode = QStringLiteral("copy");
    emit clipboardChanged();
}

void FileOperationsController::cutSelection()
{
    m_clipboardFiles = m_selection;
    m_clipboardMode = QStringLiteral("cut");
    emit clipboardChanged();
}

BackendRequestId FileOperationsController::pasteFiles(
    const QString &destination,
    const QString &conflictPolicy)
{
    if (m_clipboardFiles.isEmpty() || m_running) {
        return 0;
    }

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
    const BackendRequestId requestId = m_service->start(
        makePasteRequest(destination, conflictPolicy));
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

void FileOperationsController::resolvePasteConflict(const QString &conflictPolicy)
{
    if (!m_pasteConflictVisible) {
        return;
    }
    m_pasteConflictVisible = false;
    emit pasteConflictChanged();
    pasteFiles(m_pendingPasteDestination, conflictPolicy);
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
    emit operationStateChanged();
    setRunning(false);
    emit operationFinished(result);
}

void FileOperationsController::handleFailure(const BackendError &error)
{
    if (error.requestId != m_operationRequestId) {
        return;
    }
    m_operationError = error.message;
    m_operationStatus.clear();
    setRunning(false);
    emit operationStateChanged();
}

void FileOperationsController::setRunning(bool runningValue)
{
    if (m_running == runningValue) {
        return;
    }
    m_running = runningValue;
    emit operationStateChanged();
}

void FileOperationsController::resetOperationState()
{
    m_operationRequestId = 0;
    m_operationStatus.clear();
    setRunning(false);
    emit operationStateChanged();
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

} // namespace Astrea::Explorer::Native::Backend
