#include "services/windows_launch_controller.h"

#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>

namespace Astrea::Explorer::Native::Services {

WindowsLaunchController::WindowsLaunchController(QObject *parent)
    : QObject(parent)
{
    connect(
        &m_process,
        &QProcess::readyReadStandardOutput,
        this,
        &WindowsLaunchController::readStandardOutput);
    connect(
        &m_process,
        &QProcess::readyReadStandardError,
        this,
        &WindowsLaunchController::readStandardError);
    connect(
        &m_process,
        qOverload<int, QProcess::ExitStatus>(&QProcess::finished),
        this,
        &WindowsLaunchController::handleFinished);
    connect(
        &m_process,
        &QProcess::errorOccurred,
        this,
        &WindowsLaunchController::handleProcessError);
}

bool WindowsLaunchController::launch(const LaunchSpec &spec)
{
    if (m_process.state() != QProcess::NotRunning || m_running) {
        fail(QStringLiteral("windows_launch_already_running"));
        return false;
    }
    if (!spec.isValid()) {
        fail(QStringLiteral("invalid_launch_spec"));
        return false;
    }
    if (spec.arguments.size() != 2
        || spec.arguments.at(0) != QStringLiteral("--windows")
        || spec.arguments.at(1).isEmpty()) {
        fail(QStringLiteral("invalid_windows_launch_spec"));
        return false;
    }

    const QFileInfo programInfo(spec.program);
    if (programInfo.isAbsolute() && !programInfo.isExecutable()) {
        fail(QStringLiteral("launcher_missing"));
        return false;
    }

    m_standardOutput.clear();
    m_standardError.clear();
    m_requestedPath = spec.arguments.at(1);
    m_status = QStringLiteral("Planning Windows launch...");
    m_error.clear();
    m_runner.clear();
    m_machine.clear();
    m_warnings.clear();
    m_running = true;
    emit stateChanged();

    m_process.setProgram(spec.program);
    m_process.setArguments(spec.arguments);
    m_process.setProcessChannelMode(QProcess::SeparateChannels);
    m_process.start();
    return true;
}

bool WindowsLaunchController::running() const
{
    return m_running;
}

QString WindowsLaunchController::status() const
{
    return m_status;
}

QString WindowsLaunchController::error() const
{
    return m_error;
}

QString WindowsLaunchController::runner() const
{
    return m_runner;
}

QString WindowsLaunchController::machine() const
{
    return m_machine;
}

QStringList WindowsLaunchController::warnings() const
{
    return m_warnings;
}

void WindowsLaunchController::readStandardOutput()
{
    appendBounded(m_standardOutput, m_process.readAllStandardOutput());
}

void WindowsLaunchController::readStandardError()
{
    appendBounded(m_standardError, m_process.readAllStandardError());
}

void WindowsLaunchController::handleFinished(int exitCode, QProcess::ExitStatus exitStatus)
{
    readStandardOutput();
    readStandardError();
    if (!m_running) {
        return;
    }
    if (exitStatus != QProcess::NormalExit || exitCode != 0) {
        const QString error = QString::fromUtf8(m_standardError).trimmed();
        fail(error.isEmpty()
                 ? QStringLiteral("astrea-launch exited with code %1").arg(exitCode)
                 : error);
        return;
    }
    consumeSuccessRecord();
}

void WindowsLaunchController::handleProcessError(QProcess::ProcessError error)
{
    if (!m_running) {
        return;
    }
    if (error == QProcess::FailedToStart) {
        fail(m_process.errorString().isEmpty()
                 ? QStringLiteral("launcher_start_failed")
                 : m_process.errorString());
    }
}

void WindowsLaunchController::appendBounded(QByteArray &destination, const QByteArray &chunk)
{
    const int remaining = kMaxOutputBytes - destination.size();
    if (remaining > 0) {
        destination.append(chunk.constData(), qMin(remaining, chunk.size()));
    }
}

void WindowsLaunchController::fail(const QString &message)
{
    m_running = false;
    m_status = QStringLiteral("Windows launch failed");
    m_error = message.left(kMaxOutputBytes);
    m_runner.clear();
    m_machine.clear();
    m_warnings.clear();
    emit stateChanged();
}

void WindowsLaunchController::consumeSuccessRecord()
{
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(m_standardOutput, &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        fail(QStringLiteral("windows launcher protocol failure: malformed JSON"));
        return;
    }

    const QJsonObject record = document.object();
    const QJsonObject windows = record.value(QStringLiteral("windows")).toObject();
    const QString kind = record.value(QStringLiteral("kind")).toString();
    const QString target = record.value(QStringLiteral("target")).toString();
    const QString recordStatus = record.value(QStringLiteral("status")).toString();
    const QString runner = windows.value(QStringLiteral("runner")).toString();
    const QString machine = windows.value(QStringLiteral("machine")).toString();
    const QJsonValue warningValue = windows.value(QStringLiteral("warnings"));
    if (kind != QStringLiteral("windows") || target != m_requestedPath
        || recordStatus != QStringLiteral("ok") || runner.isEmpty() || machine.isEmpty()
        || !warningValue.isArray()) {
        fail(QStringLiteral("windows launcher protocol failure: unexpected LaunchRecord"));
        return;
    }

    QStringList warnings;
    for (const QJsonValue &warning : warningValue.toArray()) {
        if (!warning.isString()) {
            fail(QStringLiteral("windows launcher protocol failure: invalid warnings"));
            return;
        }
        warnings.append(warning.toString());
    }

    m_running = false;
    m_error.clear();
    m_runner = runner;
    m_machine = machine;
    m_warnings = warnings;
    m_status = runner == QStringLiteral("umu-proton")
        ? QStringLiteral("Opening Windows application via UMU")
        : runner == QStringLiteral("wine")
        ? QStringLiteral("Opening Windows application via Wine")
        : QStringLiteral("Opening Windows application via %1").arg(runner);
    emit stateChanged();
}

} // namespace Astrea::Explorer::Native::Services
