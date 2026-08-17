#pragma once

#include <QVariantList>

#include "backend/backend_types.h"

namespace Astrea::Explorer::Native::Services {
class FileOperationService;
class ClipboardService;
}

namespace Astrea::Explorer::Native::Backend {

class FileOperationsController final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QStringList clipboardFiles READ clipboardFiles NOTIFY clipboardChanged)
    Q_PROPERTY(QString clipboardMode READ clipboardMode NOTIFY clipboardChanged)
    Q_PROPERTY(bool running READ running NOTIFY operationStateChanged)
    Q_PROPERTY(double operationProgress READ operationProgress NOTIFY operationStateChanged)
    Q_PROPERTY(int operationPercent READ operationPercent NOTIFY operationStateChanged)
    Q_PROPERTY(QString operationFileName READ operationFileName NOTIFY operationStateChanged)
    Q_PROPERTY(QString operationStatus READ operationStatus NOTIFY operationStateChanged)
    Q_PROPERTY(QString operationError READ operationError NOTIFY operationStateChanged)
    Q_PROPERTY(QString operationDestination READ operationDestination NOTIFY operationStateChanged)
    Q_PROPERTY(int operationDoneCount READ operationDoneCount NOTIFY operationStateChanged)
    Q_PROPERTY(int operationTotalCount READ operationTotalCount NOTIFY operationStateChanged)
    Q_PROPERTY(QString operationMode READ operationMode NOTIFY operationStateChanged)
    Q_PROPERTY(QString operationState READ operationState NOTIFY operationStateChanged)
    Q_PROPERTY(QVariantList operationItems READ operationItems NOTIFY operationStateChanged)
    Q_PROPERTY(bool pasteConflictVisible READ pasteConflictVisible NOTIFY pasteConflictChanged)
    Q_PROPERTY(QVariantList pasteConflictItems READ pasteConflictItems NOTIFY pasteConflictChanged)
    Q_PROPERTY(QString pendingPasteRename READ pendingPasteRename WRITE setPendingPasteRename NOTIFY pasteConflictChanged)

public:
    explicit FileOperationsController(
        Services::FileOperationService *service,
        Services::ClipboardService *clipboardService = nullptr,
        QObject *parent = nullptr);

    QStringList clipboardFiles() const;
    QString clipboardMode() const;
    bool running() const;
    double operationProgress() const;
    int operationPercent() const;
    QString operationFileName() const;
    QString operationStatus() const;
    QString operationError() const;
    QString operationDestination() const;
    int operationDoneCount() const;
    int operationTotalCount() const;
    QString operationMode() const;
    QString operationState() const;
    QVariantList operationItems() const;
    bool pasteConflictVisible() const;
    QVariantList pasteConflictItems() const;
    QString pendingPasteRename() const;
    BackendRequestId operationRequestId() const;

    Q_INVOKABLE void setSelection(const QStringList &paths);
    Q_INVOKABLE void copySelection();
    Q_INVOKABLE void cutSelection();
    Q_INVOKABLE BackendRequestId pasteFiles(
        const QString &destination,
        const QString &conflictPolicy = QStringLiteral("keep-both"));
    Q_INVOKABLE BackendRequestId transferFiles(
        const QStringList &sources,
        const QString &destination,
        const QString &mode = QStringLiteral("copy"),
        const QString &conflictPolicy = QStringLiteral("keep-both"));
    Q_INVOKABLE void cancelOperation();
    Q_INVOKABLE bool isCutPending(const QString &name) const;
    Q_INVOKABLE bool isCutPathPending(const QString &path) const;
    Q_INVOKABLE void resolvePasteConflict(const QString &conflictPolicy);
    Q_INVOKABLE void renamePasteConflict(const QString &name);
    Q_INVOKABLE void cancelPasteConflict();

    void setPendingPasteRename(const QString &name);

signals:
    void clipboardChanged();
    void imagePasted(const QString &path);
    void operationStateChanged();
    void pasteConflictChanged();
    void operationFinished(
        const Astrea::Explorer::Native::Backend::FileOperationResult &result);

private slots:
    void handleProgress(
        BackendRequestId requestId,
        const FileOperationProgress &progress);
    void handleFinished(
        BackendRequestId requestId,
        const FileOperationResult &result);
    void handleFailure(const BackendError &error);

private:
    void setRunning(bool running);
    void resetOperationState();
    Backend::FileOperationRequest makePasteRequest(
        const QString &destination,
        const QString &conflictPolicy) const;
    Backend::FileOperationRequest makeTransferRequest(
        const QStringList &sources,
        const QString &destination,
        const QString &mode,
        const QString &conflictPolicy) const;
    QVariantList findConflicts(
        const QStringList &sources,
        const QString &destination) const;
    bool showConflictPrompt(
        const QStringList &sources,
        const QString &destination,
        const QString &mode);

    Services::FileOperationService *m_service = nullptr;
    Services::ClipboardService *m_clipboardService = nullptr;
    QStringList m_selection;
    QStringList m_clipboardFiles;
    QString m_clipboardMode {QStringLiteral("copy")};
    bool m_running = false;
    double m_operationProgress = 0.0;
    int m_operationPercent = 0;
    QString m_operationFileName;
    QString m_operationStatus;
    QString m_operationError;
    QString m_operationDestination;
    int m_operationDoneCount = 0;
    int m_operationTotalCount = 0;
    QString m_operationMode;
    QString m_operationState;
    QVariantList m_operationItems;
    BackendRequestId m_operationRequestId = 0;
    bool m_pasteConflictVisible = false;
    QVariantList m_pasteConflictItems;
    QString m_pendingPasteRename;
    QString m_pendingPasteDestination;
    QStringList m_pendingTransferSources;
    QString m_pendingTransferMode;
    bool m_conflictResolutionInProgress = false;
};

} // namespace Astrea::Explorer::Native::Backend
