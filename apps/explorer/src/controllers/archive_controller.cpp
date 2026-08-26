#include "controllers/archive_controller.h"

#include <QDir>
#include <QFileInfo>
#include <QJsonValue>

#include "controllers/navigation_controller.h"
#include "services/filesystem_service.h"

namespace Astrea::Explorer::Native::Backend {

ArchiveController::ArchiveController(
    Services::FilesystemService *filesystem,
    NavigationController *navigation,
    QObject *parent)
    : QObject(parent)
    , m_filesystem(filesystem)
    , m_navigation(navigation)
{
    Q_ASSERT(m_filesystem != nullptr);
    connect(
        m_filesystem,
        &Services::FilesystemService::operationFinished,
        this,
        &ArchiveController::handleFilesystemResult);
}

bool ArchiveController::running() const
{
    return m_running;
}

double ArchiveController::progress() const
{
    return m_progress;
}

int ArchiveController::percent() const
{
    return m_percent;
}

QString ArchiveController::fileName() const
{
    return m_fileName;
}

QString ArchiveController::status() const
{
    return m_status;
}

QString ArchiveController::error() const
{
    return m_error;
}

QString ArchiveController::destination() const
{
    return m_running || m_destinationResult.isEmpty() ? m_destination : m_destinationResult;
}

QString ArchiveController::destinationResult() const
{
    return m_destinationResult;
}

int ArchiveController::doneCount() const
{
    return m_doneCount;
}

int ArchiveController::totalCount() const
{
    return m_totalCount;
}

QString ArchiveController::remainingText() const
{
    return m_running ? QStringLiteral("Aguardando...") : QString();
}

bool ArchiveController::passwordPromptVisible() const
{
    return m_passwordPrompt;
}

QString ArchiveController::passwordError() const
{
    return m_passwordError;
}

bool ArchiveController::conflictVisible() const
{
    return m_conflict;
}

QString ArchiveController::conflictDestination() const
{
    return m_conflictDestination;
}

QString ArchiveController::conflictName() const
{
    return m_conflictName;
}

QString ArchiveController::conflictPolicy() const
{
    return m_conflictPolicy;
}

BackendRequestId ArchiveController::request() const
{
    return m_request;
}

int ArchiveController::stateRevision() const
{
    return m_stateRevision;
}

bool ArchiveController::workflowOccupied() const
{
    return m_running || m_passwordPrompt || m_conflict;
}

void ArchiveController::startArchiveExtraction(const QString &path, const QString &folderName)
{
    if (workflowOccupied() || m_filesystem == nullptr || path.isEmpty()) {
        return;
    }

    m_path = path;
    const QString defaultName = QFileInfo(path).completeBaseName();
    m_destination = QDir(m_navigation == nullptr ? QString() : m_navigation->currentPath())
                        .filePath(folderName.isEmpty() ? defaultName : folderName);
    m_conflictPolicy = QStringLiteral("keep-both");
    m_fileName = QFileInfo(path).fileName();
    m_status = QStringLiteral("Extraindo...");
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
    m_running = true;
    publishState();
    m_request = m_filesystem->archiveExtract(
        m_path, m_destination, QString(), m_conflictPolicy);
}

void ArchiveController::startPasswordContinuation(const QString &password)
{
    m_passwordError.clear();
    m_error.clear();
    m_destinationResult.clear();
    m_status = QStringLiteral("Extraindo...");
    m_percent = 0;
    m_progress = 0.0;
    m_doneCount = 0;
    m_totalCount = 0;
    m_running = true;
    publishState();
    m_request = m_filesystem->archiveExtract(
        m_path, m_destination, password, m_conflictPolicy);
}

void ArchiveController::submitArchivePassword(const QString &password)
{
    if (m_filesystem == nullptr || m_path.isEmpty() || !m_passwordPrompt || m_running) {
        return;
    }
    m_passwordPrompt = false;
    startPasswordContinuation(password);
}

void ArchiveController::cancelArchivePassword()
{
    if (!m_passwordPrompt) {
        return;
    }
    m_passwordPrompt = false;
    publishState();
}

void ArchiveController::submitArchiveConflict(const QString &policy)
{
    if (m_filesystem == nullptr || m_path.isEmpty() || !m_conflict || m_running
        || !isSupportedConflictPolicy(policy)) {
        return;
    }
    m_conflictPolicy = policy;
    m_conflict = false;
    startPasswordContinuation(QString());
}

void ArchiveController::cancelArchiveConflict()
{
    if (!m_conflict) {
        return;
    }
    m_conflict = false;
    publishState();
}

void ArchiveController::startFolderCompression(const QString &path, const QString &format)
{
    if (workflowOccupied() || m_filesystem == nullptr || path.isEmpty()) {
        return;
    }

    const QString suffix = format.isEmpty() ? QStringLiteral("tar.gz") : format;
    m_path = path;
    m_fileName = QFileInfo(path).fileName();
    m_destination = QDir(m_navigation == nullptr ? QString() : m_navigation->currentPath())
                        .filePath(m_fileName + QStringLiteral(".") + suffix);
    m_status = QStringLiteral("Comprimindo...");
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
    m_running = true;
    publishState();
    m_request = m_filesystem->archiveCompress(path, m_destination, suffix);
}

void ArchiveController::handleFilesystemResult(const UtilityResult &result)
{
    if ((result.operation != QStringLiteral("archive-extract")
         && result.operation != QStringLiteral("archive-compress"))
        || result.requestId != m_request) {
        return;
    }

    const BackendRequestId completedRequest = m_request;
    const QString completedOperation = result.operation;
    m_running = false;
    m_percent = result.ok ? 100 : 0;
    m_progress = result.ok ? 1.0 : 0.0;
    m_doneCount = result.ok ? 1 : 0;
    m_totalCount = result.ok ? 1 : 0;
    m_error = result.ok ? QString() : result.errorMessage;
    m_status = result.ok ? QStringLiteral("Concluído") : QStringLiteral("Falha");
    if (result.ok) {
        QJsonValue resultPath = result.data.value(QStringLiteral("destination"));
        if (!resultPath.isString()) {
            resultPath = result.data.value(QStringLiteral("path"));
        }
        m_destinationResult = resultPath.toString();
        if (completedOperation == QStringLiteral("archive-extract")
            && !m_destinationResult.isEmpty() && m_navigation != nullptr) {
            m_navigation->navigateTo(m_destinationResult);
        }
    } else {
        m_destinationResult.clear();
    }

    // Release the native slot before publishing state. A stateChanged handler
    // may synchronously start the next archive, so the completion signal uses
    // the local identity and never clears a newer request.
    m_request = 0;
    publishState();
    emit operationFinished(
        completedRequest,
        completedOperation,
        result.ok,
        result.data.toVariantMap(),
        result.ok ? QString() : result.errorMessage);
}

void ArchiveController::publishState()
{
    ++m_stateRevision;
    emit stateChanged();
}

bool ArchiveController::isSupportedConflictPolicy(const QString &policy)
{
    return policy == QStringLiteral("keep-both") || policy == QStringLiteral("overwrite")
        || policy == QStringLiteral("rename");
}

} // namespace Astrea::Explorer::Native::Backend
