#pragma once

#include <QObject>
#include <QVariantMap>

#include "backend/backend_types.h"

namespace Astrea::Explorer::Native::Backend {

class NavigationController;
}

namespace Astrea::Explorer::Native::Services {

class FilesystemService;
}

namespace Astrea::Explorer::Native::Backend {

class ArchiveController final : public QObject
{
    Q_OBJECT

public:
    explicit ArchiveController(
        Services::FilesystemService *filesystem,
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
    QString remainingText() const;
    bool passwordPromptVisible() const;
    QString passwordError() const;
    bool conflictVisible() const;
    QString conflictDestination() const;
    QString conflictName() const;
    QString conflictPolicy() const;
    BackendRequestId request() const;
    int stateRevision() const;
    bool workflowOccupied() const;

    void startArchiveExtraction(const QString &path, const QString &folderName);
    void submitArchivePassword(const QString &password);
    void cancelArchivePassword();
    void submitArchiveConflict(const QString &policy);
    void cancelArchiveConflict();
    void startFolderCompression(const QString &path, const QString &format);

signals:
    void stateChanged();
    void operationFinished(
        Astrea::Explorer::Native::Backend::BackendRequestId requestId,
        const QString &operation,
        bool ok,
        const QVariantMap &data,
        const QString &error);

private:
    void handleFilesystemResult(const UtilityResult &result);
    void startPasswordContinuation(const QString &password);
    void publishState();
    static bool isSupportedConflictPolicy(const QString &policy);

    Services::FilesystemService *m_filesystem = nullptr;
    NavigationController *m_navigation = nullptr;
    BackendRequestId m_request = 0;
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
    QString m_fileName;
    QString m_status;
    QString m_error;
    QString m_destinationResult;
    QString m_passwordError;
    QString m_conflictDestination;
    QString m_conflictName;
    int m_stateRevision = 0;
};

} // namespace Astrea::Explorer::Native::Backend
