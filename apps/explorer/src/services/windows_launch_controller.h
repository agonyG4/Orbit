#pragma once

#include <QByteArray>
#include <QObject>
#include <QProcess>
#include <QString>
#include <QStringList>

#include "services/launch_service.h"

namespace Astrea::Explorer::Native::Services {

class WindowsLaunchController final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool running READ running NOTIFY stateChanged)
    Q_PROPERTY(QString status READ status NOTIFY stateChanged)
    Q_PROPERTY(QString error READ error NOTIFY stateChanged)
    Q_PROPERTY(QString runner READ runner NOTIFY stateChanged)
    Q_PROPERTY(QString machine READ machine NOTIFY stateChanged)
    Q_PROPERTY(QStringList warnings READ warnings NOTIFY stateChanged)

public:
    explicit WindowsLaunchController(QObject *parent = nullptr);

    bool launch(const LaunchSpec &spec);

    bool running() const;
    QString status() const;
    QString error() const;
    QString runner() const;
    QString machine() const;
    QStringList warnings() const;

signals:
    void stateChanged();

private slots:
    void readStandardOutput();
    void readStandardError();
    void handleFinished(int exitCode, QProcess::ExitStatus exitStatus);
    void handleProcessError(QProcess::ProcessError error);

private:
    static constexpr int kMaxOutputBytes = 64 * 1024;

    void appendBounded(QByteArray &destination, const QByteArray &chunk);
    void fail(const QString &message);
    void consumeSuccessRecord();

    QProcess m_process;
    QByteArray m_standardOutput;
    QByteArray m_standardError;
    QString m_requestedPath;
    bool m_running = false;
    QString m_status;
    QString m_error;
    QString m_runner;
    QString m_machine;
    QStringList m_warnings;
};

} // namespace Astrea::Explorer::Native::Services
