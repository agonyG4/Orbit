#pragma once

#include <QObject>
#include <QVariantList>

#include "backend/backend_types.h"

namespace Astrea::Explorer::Native::Backend {

class NavigationController;
}

namespace Astrea::Explorer::Native::Services {

class ArchiveOperationService;
}

namespace Astrea::Explorer::Native::Backend {

class ArchiveController final : public QObject
{
    Q_OBJECT

public:
    explicit ArchiveController(
        Services::ArchiveOperationService *service,
        NavigationController *navigation,
        QObject *parent = nullptr);

    bool running() const;
    double progress() const;
    int percent() const;
    QString fileName() const;
    QString status() const;
    QString error() const;
    QString destination() const;
    QString destinationResult() const;
    int doneCount() const;
    int totalCount() const;
    qint64 bytesDone() const;
    qint64 bytesTotal() const;
    QString phase() const;
    QString currentPath() const;
    QString currentName() const;
    QString remainingText() const;
    QString operationKind() const;
    QVariantList capabilities() const;
    bool passwordPromptVisible() const;
    QString passwordError() const;
    bool conflictVisible() const;
    QString conflictDestination() const;
    QString conflictName() const;
    QString conflictPolicy() const;
    BackendRequestId request() const;
    int stateRevision() const;
    bool workflowOccupied() const;

    void refreshCapabilities();
    bool canExtractArchive(const QString &path) const;

    void startArchiveExtraction(const QString &path, const QString &folderName);
    void startArchiveExtractionTo(const QString &path, const QString &destination);
    void startArchiveCreation(
        const QStringList &sources,
        const QString &archiveName,
        const QString &format,
        const QString &profile);
    void submitArchivePassword(const QString &password);
    void cancelArchivePassword();
    void submitArchiveConflict(const QString &policy);
    void cancelArchiveConflict();
    void cancelArchiveOperation();

    // Compatibility wrapper for existing non-QML callers during migration.
    void startFolderCompression(const QString &path, const QString &format);

signals:
    void stateChanged();
    void capabilitiesChanged();
    void operationFinished(
        Astrea::Explorer::Native::Backend::BackendRequestId requestId,
        const QString &operation,
        bool ok,
        const QVariantMap &data,
        const QString &error);

private slots:
    void handleProgress(
        BackendRequestId requestId,
        const ArchiveOperationProgress &progress);
    void handleFinished(
        BackendRequestId requestId,
        const ArchiveOperationResult &result);
    void handleFailure(const BackendError &error);

private:
    void resetForStart(const QString &operation, const QString &fileName);
    BackendRequestId startRequest(const ArchiveOperationRequest &request);
    void startPasswordContinuation(const QString &password);
    void publishState();
    void setCapabilities(const QVector<ArchiveCapability> &capabilities);
    static bool isSupportedConflictPolicy(const QString &policy);
    static QString extensionForFormat(const QString &format);
    static QVariantMap resultMap(const ArchiveOperationResult &result);

    Services::ArchiveOperationService *m_service = nullptr;
    NavigationController *m_navigation = nullptr;
    BackendRequestId m_request = 0;
    ArchiveOperationRequest m_workflow;
    QString m_operationKind;
    QString m_path;
    QString m_destination;
    QString m_conflictPolicy {QStringLiteral("keep-both")};
    bool m_running = false;
    bool m_passwordPrompt = false;
    bool m_conflict = false;
    double m_progress = 0.0;
    int m_percent = 0;
    int m_doneCount = 0;
    int m_totalCount = 0;
    qint64 m_bytesDone = -1;
    qint64 m_bytesTotal = -1;
    QString m_phase;
    QString m_currentPath;
    QString m_currentName;
    QString m_fileName;
    QString m_status;
    QString m_error;
    QString m_destinationResult;
    QString m_passwordError;
    QString m_conflictDestination;
    QString m_conflictName;
    QVector<ArchiveCapability> m_capabilities;
    int m_stateRevision = 0;
};

} // namespace Astrea::Explorer::Native::Backend
