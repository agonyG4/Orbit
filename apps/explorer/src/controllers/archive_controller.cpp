#include "controllers/archive_controller.h"

#include <QDir>
#include <QFileInfo>

#include "controllers/navigation_controller.h"
#include "services/archive_operation_service.h"

namespace Astrea::Explorer::Native::Backend {

ArchiveController::ArchiveController(
    Services::ArchiveOperationService *service,
    NavigationController *navigation,
    QObject *parent)
    : QObject(parent)
    , m_service(service)
    , m_navigation(navigation)
{
    Q_ASSERT(m_service != nullptr);
    connect(m_service, &Services::ArchiveOperationService::progress,
        this, &ArchiveController::handleProgress);
    connect(m_service, &Services::ArchiveOperationService::finished,
        this, &ArchiveController::handleFinished);
    connect(m_service, &Services::ArchiveOperationService::failed,
        this, &ArchiveController::handleFailure);
}

bool ArchiveController::running() const { return m_running; }
double ArchiveController::progress() const { return m_progress; }
int ArchiveController::percent() const { return m_percent; }
QString ArchiveController::fileName() const { return m_fileName; }
QString ArchiveController::status() const { return m_status; }
QString ArchiveController::error() const { return m_error; }

QString ArchiveController::destination() const
{
    return m_running || m_destinationResult.isEmpty() ? m_destination : m_destinationResult;
}

QString ArchiveController::destinationResult() const { return m_destinationResult; }
int ArchiveController::doneCount() const { return m_doneCount; }
int ArchiveController::totalCount() const { return m_totalCount; }
qint64 ArchiveController::bytesDone() const { return m_bytesDone; }
qint64 ArchiveController::bytesTotal() const { return m_bytesTotal; }
QString ArchiveController::phase() const { return m_phase; }
QString ArchiveController::currentPath() const { return m_currentPath; }
QString ArchiveController::currentName() const { return m_currentName; }

QString ArchiveController::remainingText() const
{
    if (!m_running || m_totalCount <= 0) {
        return {};
    }
    return QStringLiteral("%1 / %2").arg(m_doneCount).arg(m_totalCount);
}

QString ArchiveController::operationKind() const { return m_operationKind; }
QString ArchiveController::workflowState() const { return m_workflowState; }

QVariantList ArchiveController::capabilities() const
{
    QVariantList result;
    for (const ArchiveCapability &capability : m_capabilities) {
        QVariantMap value;
        value.insert(QStringLiteral("id"), capability.id);
        value.insert(QStringLiteral("label"), capability.label);
        value.insert(QStringLiteral("extension"), capability.extension);
        value.insert(QStringLiteral("createSupported"), capability.createSupported);
        value.insert(QStringLiteral("extractSupported"), capability.extractSupported);
        value.insert(QStringLiteral("profiles"), capability.profiles);
        value.insert(QStringLiteral("createProvider"), capability.createProvider);
        value.insert(QStringLiteral("extractProvider"), capability.extractProvider);
        value.insert(QStringLiteral("createPasswordSupported"), capability.createPasswordSupported);
        value.insert(QStringLiteral("extractPasswordSupported"), capability.extractPasswordSupported);
        result.append(value);
    }
    return result;
}

bool ArchiveController::passwordPromptVisible() const { return m_passwordPrompt; }
QString ArchiveController::passwordError() const { return m_passwordError; }
bool ArchiveController::conflictVisible() const { return m_conflict; }
QString ArchiveController::conflictDestination() const { return m_conflictDestination; }
QString ArchiveController::conflictName() const { return m_conflictName; }
QString ArchiveController::conflictPolicy() const { return m_conflictPolicy; }
BackendRequestId ArchiveController::request() const { return m_request; }
int ArchiveController::stateRevision() const { return m_stateRevision; }

bool ArchiveController::workflowOccupied() const
{
    return m_running || m_passwordPrompt || m_conflict;
}

void ArchiveController::refreshCapabilities()
{
    if (workflowOccupied()) {
        return;
    }
    ArchiveOperationRequest request;
    request.kind = QStringLiteral("capabilities");
    if (startRequest(request) == 0) {
        return;
    }
    m_operationKind = QStringLiteral("capabilities");
    m_running = true;
    m_status = QStringLiteral("Checking archive capabilities...");
    publishState();
}

bool ArchiveController::canExtractArchive(const QString &path) const
{
    const QString lower = path.toLower();
    for (const ArchiveCapability &capability : m_capabilities) {
        if (!capability.extractSupported) {
            continue;
        }
        const QString extension = QStringLiteral(".") + capability.extension.toLower();
        if (lower.endsWith(extension)) {
            return true;
        }
        if (capability.id == QStringLiteral("tar.gz") && lower.endsWith(QStringLiteral(".tgz"))) {
            return true;
        }
        if (capability.id == QStringLiteral("tar.xz") && lower.endsWith(QStringLiteral(".txz"))) {
            return true;
        }
        if (capability.id == QStringLiteral("tar.zst") && lower.endsWith(QStringLiteral(".tzst"))) {
            return true;
        }
        if (capability.id == QStringLiteral("tar.bz2")
            && (lower.endsWith(QStringLiteral(".tbz2")) || lower.endsWith(QStringLiteral(".tar.bz2")))) {
            return true;
        }
    }
    return false;
}

QString ArchiveController::canonicalArchiveStem(const QString &name)
{
    const QString fileName = QFileInfo(name).fileName();
    static const QStringList suffixes {
        QStringLiteral(".tar.zst"),
        QStringLiteral(".tar.bz2"),
        QStringLiteral(".tar.xz"),
        QStringLiteral(".tar.gz"),
        QStringLiteral(".tzst"),
        QStringLiteral(".tbz2"),
        QStringLiteral(".txz"),
        QStringLiteral(".tgz"),
        QStringLiteral(".7z"),
        QStringLiteral(".zip"),
        QStringLiteral(".tar"),
        QStringLiteral(".rar"),
    };
    for (const QString &suffix : suffixes) {
        if (fileName.endsWith(suffix, Qt::CaseInsensitive)) {
            return fileName.left(fileName.size() - suffix.size());
        }
    }
    return fileName;
}

void ArchiveController::resetForStart(const QString &operation, const QString &fileName)
{
    m_operationKind = operation;
    m_fileName = fileName;
    m_status = operation == QStringLiteral("create")
        ? QStringLiteral("Compressing...")
        : QStringLiteral("Extracting...");
    m_error.clear();
    m_destinationResult.clear();
    m_passwordError.clear();
    m_passwordPrompt = false;
    m_conflict = false;
    m_conflictDestination.clear();
    m_conflictName.clear();
    m_percent = 0;
    m_progress = 0.0;
    m_doneCount = 0;
    m_totalCount = 0;
    m_bytesDone = -1;
    m_bytesTotal = -1;
    m_phase.clear();
    m_currentPath.clear();
    m_currentName.clear();
    m_running = true;
    m_workflowState = QStringLiteral("running");
    m_workflowRequest = 0;
}

BackendRequestId ArchiveController::startRequest(const ArchiveOperationRequest &request)
{
    m_workflow = request;
    m_conflictPolicy = request.conflictPolicy;
    const BackendRequestId requestId = m_service->start(request);
    if (requestId != 0) {
        m_request = requestId;
        if ((request.kind == QStringLiteral("create")
             || request.kind == QStringLiteral("extract"))
            && m_workflowRequest == 0) {
            m_workflowRequest = requestId;
        }
    }
    return requestId;
}

void ArchiveController::startArchiveExtraction(const QString &path, const QString &folderName)
{
    if (path.isEmpty()) {
        return;
    }
    const QString defaultName = canonicalArchiveStem(path);
    const QString currentPath = m_navigation == nullptr ? QString() : m_navigation->currentPath();
    const QString destination = QDir(currentPath).filePath(
        folderName.isEmpty() ? defaultName : folderName);
    startArchiveExtractionRequest(path, destination, QStringLiteral("new-directory"));
}

void ArchiveController::startArchiveExtractionRequest(
    const QString &path,
    const QString &destination,
    const QString &destinationMode)
{
    if (workflowOccupied() || path.isEmpty() || destination.isEmpty()
        || (!m_capabilities.isEmpty() && !canExtractArchive(path))) {
        return;
    }
    ArchiveOperationRequest request;
    request.kind = QStringLiteral("extract");
    request.archivePath = path;
    request.destination = destination;
    request.conflictPolicy = QStringLiteral("prompt");
    request.destinationMode = destinationMode;
    m_path = path;
    m_destination = destination;
    resetForStart(QStringLiteral("extract"), QFileInfo(path).fileName());
    publishState();
    if (startRequest(request) == 0) {
        m_running = false;
        m_workflowState = QStringLiteral("failed");
        publishState();
    }
}

void ArchiveController::startArchiveExtractionHere(
    const QString &path,
    const QString &destination)
{
    if (workflowOccupied() || path.isEmpty() || destination.isEmpty()
        || (!m_capabilities.isEmpty() && !canExtractArchive(path))) {
        return;
    }
    ArchiveOperationRequest request;
    request.kind = QStringLiteral("extract");
    request.archivePath = path;
    request.destination = destination;
    request.conflictPolicy = QStringLiteral("prompt");
    request.destinationMode = QStringLiteral("into-directory");
    m_path = path;
    m_destination = destination;
    resetForStart(QStringLiteral("extract"), QFileInfo(path).fileName());
    publishState();
    if (startRequest(request) == 0) {
        m_running = false;
        m_workflowState = QStringLiteral("failed");
        publishState();
    }
}

void ArchiveController::startArchiveExtractionTo(
    const QString &path,
    const QString &destination)
{
    startArchiveExtractionRequest(path, destination, QStringLiteral("into-directory"));
}

void ArchiveController::startArchiveCreation(
    const QStringList &sources,
    const QString &archiveName,
    const QString &format,
    const QString &profile)
{
    if (workflowOccupied() || sources.isEmpty() || format.isEmpty()) {
        return;
    }
    const QString baseName = archiveName.trimmed().isEmpty()
        ? (sources.size() == 1 ? QFileInfo(sources.constFirst()).fileName() : QStringLiteral("Archive"))
        : archiveName.trimmed();
    if (baseName == QStringLiteral(".") || baseName == QStringLiteral("..")
        || baseName.contains(QChar('/')) || baseName.contains(QChar('\\'))) {
        return;
    }
    const QString currentPath = m_navigation == nullptr ? QString() : m_navigation->currentPath();
    const QString destination = QDir(currentPath).filePath(baseName + extensionForFormat(format));
    ArchiveOperationRequest request;
    request.kind = QStringLiteral("create");
    request.sources = sources;
    request.archivePath = QDir(currentPath).filePath(baseName);
    request.format = format;
    request.profile = profile.isEmpty() ? QStringLiteral("balanced") : profile;
    request.conflictPolicy = QStringLiteral("keep-both");
    request.destinationMode = QStringLiteral("new-directory");
    m_path = sources.constFirst();
    m_destination = destination;
    resetForStart(QStringLiteral("create"), baseName);
    publishState();
    if (startRequest(request) == 0) {
        m_running = false;
        m_workflowState = QStringLiteral("failed");
        publishState();
    }
}

void ArchiveController::startPasswordContinuation(const QString &password)
{
    m_workflow.password = password;
    m_passwordError.clear();
    m_error.clear();
    m_destinationResult.clear();
    m_percent = 0;
    m_progress = 0.0;
    m_doneCount = 0;
    m_totalCount = 0;
    m_bytesDone = -1;
    m_bytesTotal = -1;
    m_running = true;
    m_workflowState = QStringLiteral("running");
    m_passwordPrompt = false;
    m_status = QStringLiteral("Extracting...");
    publishState();
    if (startRequest(m_workflow) == 0) {
        m_running = false;
        m_passwordPrompt = true;
        m_workflowState = QStringLiteral("waiting-password");
        publishState();
    }
}

void ArchiveController::submitArchivePassword(const QString &password)
{
    if (!m_passwordPrompt || m_running || m_path.isEmpty()) {
        return;
    }
    startPasswordContinuation(password);
}

void ArchiveController::cancelArchivePassword()
{
    if (!m_passwordPrompt) {
        return;
    }
    finishWaitingWorkflowAsCancelled();
}

void ArchiveController::submitArchiveConflict(const QString &policy)
{
    if (!m_conflict || m_running || !isSupportedConflictPolicy(policy)) {
        return;
    }
    m_conflictPolicy = policy;
    m_conflict = false;
    m_workflow.conflictPolicy = policy;
    startPasswordContinuation(m_workflow.password);
}

void ArchiveController::cancelArchiveConflict()
{
    if (!m_conflict) {
        return;
    }
    finishWaitingWorkflowAsCancelled();
}

void ArchiveController::finishWaitingWorkflowAsCancelled()
{
    const BackendRequestId completedRequest = m_workflowRequest != 0
        ? m_workflowRequest
        : m_request;
    m_passwordPrompt = false;
    m_conflict = false;
    m_workflow.password.clear();
    m_request = 0;
    m_workflowRequest = 0;
    m_workflowState = QStringLiteral("cancelled");
    m_status = QStringLiteral("Cancelled");
    m_error.clear();
    publishState();
    QVariantMap data;
    data.insert(QStringLiteral("state"), QStringLiteral("cancelled"));
    emit operationFinished(
        completedRequest,
        QStringLiteral("extract"),
        false,
        data,
        QString());
}

void ArchiveController::cancelArchiveOperation()
{
    if (!m_running || m_request == 0) {
        return;
    }
    m_service->cancel(m_request);
}

void ArchiveController::startFolderCompression(const QString &path, const QString &format)
{
    startArchiveCreation({path}, QFileInfo(path).fileName(), format, QStringLiteral("balanced"));
}

void ArchiveController::handleProgress(
    BackendRequestId requestId,
    const ArchiveOperationProgress &progress)
{
    if (requestId != m_request) {
        return;
    }
    m_operationKind = progress.operation;
    m_phase = progress.phase;
    m_doneCount = progress.doneCount;
    m_totalCount = progress.totalCount;
    m_bytesDone = progress.bytesDone;
    m_bytesTotal = progress.bytesTotal;
    m_progress = progress.progress;
    m_percent = progress.percent;
    m_currentPath = progress.currentPath;
    m_currentName = progress.currentName;
    if (!progress.statusText.isEmpty()) {
        m_status = progress.statusText;
    }
    publishState();
}

void ArchiveController::handleFinished(
    BackendRequestId requestId,
    const ArchiveOperationResult &result)
{
    if (requestId != m_request) {
        return;
    }
    const BackendRequestId completedRequest = m_request;
    const BackendRequestId logicalRequest = m_workflowRequest != 0
        ? m_workflowRequest
        : completedRequest;
    const QString completedOperation = result.operation.isEmpty() ? m_operationKind : result.operation;
    const bool success = result.state == QStringLiteral("success");
    if (completedOperation == QStringLiteral("capabilities")) {
        setCapabilities(result.capabilities);
    }
    m_running = false;
    m_percent = result.percent;
    m_progress = result.progress;
    m_doneCount = result.doneCount;
    m_totalCount = result.totalCount;
    m_bytesDone = result.bytesDone;
    m_bytesTotal = result.bytesTotal;
    m_phase = result.phase;
    m_error = success ? QString() : result.errorMessage;
    m_status = success ? QStringLiteral("Completed") : QStringLiteral("Failed");
    if (result.state == QStringLiteral("password-required")
        || result.state == QStringLiteral("bad-password")) {
        m_workflowState = QStringLiteral("waiting-password");
        m_passwordPrompt = true;
        m_passwordError = result.state == QStringLiteral("bad-password")
            ? QStringLiteral("The password is incorrect.")
            : QString();
        m_status = QStringLiteral("Password required");
    } else if (result.state == QStringLiteral("destination-conflict")) {
        m_conflict = true;
        m_conflictDestination = result.destination.isEmpty() ? m_destination : result.destination;
        m_conflictName = QFileInfo(m_conflictDestination).fileName();
        m_status = QStringLiteral("Choose how to resolve the destination conflict");
        m_workflowState = QStringLiteral("waiting-conflict");
    } else if (result.state == QStringLiteral("cancelled")) {
        m_workflowState = QStringLiteral("cancelled");
        m_error.clear();
        m_status = QStringLiteral("Cancelled");
    } else if (success) {
        m_workflowState = QStringLiteral("success");
        m_destinationResult = result.destination;
        if (completedOperation == QStringLiteral("extract") && !m_destinationResult.isEmpty()
            && m_navigation != nullptr) {
            m_navigation->navigateTo(m_destinationResult);
        }
    } else {
        m_workflowState = QStringLiteral("failed");
    }
    m_request = 0;
    publishState();
    if (result.state == QStringLiteral("password-required")
        || result.state == QStringLiteral("bad-password")
        || result.state == QStringLiteral("destination-conflict")) {
        return;
    }
    if (completedOperation == QStringLiteral("create")
        || completedOperation == QStringLiteral("extract")) {
        m_workflow.password.clear();
        m_workflowRequest = 0;
    }
    emit operationFinished(
        logicalRequest,
        completedOperation,
        success,
        resultMap(result),
        success ? QString() : result.errorMessage);
}

void ArchiveController::handleFailure(const BackendError &error)
{
    if (error.requestId != m_request) {
        return;
    }
    ArchiveOperationResult result;
    result.requestId = error.requestId;
    result.operation = m_operationKind;
    result.state = error.code == QStringLiteral("cancelled")
        ? QStringLiteral("cancelled")
        : QStringLiteral("provider-failed");
    result.errorCode = error.code;
    result.errorMessage = error.message;
    handleFinished(error.requestId, result);
}

void ArchiveController::publishState()
{
    ++m_stateRevision;
    emit stateChanged();
}

void ArchiveController::setCapabilities(const QVector<ArchiveCapability> &capabilities)
{
    m_capabilities = capabilities;
    emit capabilitiesChanged();
}

QString ArchiveController::extensionForFormat(const QString &format)
{
    if (format == QStringLiteral("7z")) return QStringLiteral(".7z");
    if (format == QStringLiteral("tar")) return QStringLiteral(".tar");
    if (format == QStringLiteral("tar.gz") || format == QStringLiteral("tgz")) return QStringLiteral(".tar.gz");
    if (format == QStringLiteral("tar.xz") || format == QStringLiteral("txz")) return QStringLiteral(".tar.xz");
    if (format == QStringLiteral("tar.zst") || format == QStringLiteral("tzst")) return QStringLiteral(".tar.zst");
    return QStringLiteral(".zip");
}

QVariantMap ArchiveController::resultMap(const ArchiveOperationResult &result)
{
    QVariantMap map;
    map.insert(QStringLiteral("state"), result.state);
    map.insert(QStringLiteral("operation"), result.operation);
    map.insert(QStringLiteral("errorCode"), result.errorCode);
    map.insert(QStringLiteral("errorMessage"), result.errorMessage);
    map.insert(QStringLiteral("destination"), result.destination);
    map.insert(QStringLiteral("phase"), result.phase);
    map.insert(QStringLiteral("doneCount"), result.doneCount);
    map.insert(QStringLiteral("totalCount"), result.totalCount);
    map.insert(QStringLiteral("bytesDone"), result.bytesDone);
    map.insert(QStringLiteral("bytesTotal"), result.bytesTotal);
    map.insert(QStringLiteral("progress"), result.progress);
    map.insert(QStringLiteral("percent"), result.percent);
    return map;
}

bool ArchiveController::isSupportedConflictPolicy(const QString &policy)
{
    return policy == QStringLiteral("keep-both") || policy == QStringLiteral("overwrite");
}

} // namespace Astrea::Explorer::Native::Backend
