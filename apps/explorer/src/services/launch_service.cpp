#include "services/launch_service.h"

#include <utility>

#include <QFileInfo>
#include <QProcess>

namespace Astrea::Explorer::Native::Services {

bool LaunchSpec::isValid() const
{
    return !program.isEmpty();
}

LaunchService::LaunchService(QString astreaLaunchProgram, QString windowsRunProgram)
    : m_astreaLaunchProgram(std::move(astreaLaunchProgram))
    , m_windowsRunProgram(std::move(windowsRunProgram))
{
}

LaunchSpec LaunchService::fileLaunch(const QString &path) const
{
    if (path.isEmpty()) {
        return {};
    }
    return makeSpec(m_astreaLaunchProgram, {QStringLiteral("--file"), path});
}

LaunchSpec LaunchService::desktopLaunch(const QString &path) const
{
    if (path.isEmpty()) {
        return {};
    }
    return makeSpec(m_astreaLaunchProgram, {QStringLiteral("--desktop"), path});
}

LaunchSpec LaunchService::desktopLaunch(
    const QString &desktopFile,
    const QString &targetPath) const
{
    return desktopLaunch(desktopFile, {targetPath});
}

LaunchSpec LaunchService::desktopLaunch(
    const QString &desktopFile,
    const QStringList &targetFiles,
    const QStringList &targetUrls) const
{
    if (desktopFile.isEmpty()) {
        return {};
    }
    QStringList arguments {QStringLiteral("--desktop"), desktopFile};
    int targetCount = 0;
    for (const QString &path : targetFiles) {
        if (!path.isEmpty()) {
            arguments << QStringLiteral("--file") << path;
            ++targetCount;
        }
    }
    for (const QString &url : targetUrls) {
        if (!url.isEmpty()) {
            arguments << QStringLiteral("--url") << url;
            ++targetCount;
        }
    }
    if (targetCount == 0) {
        return {};
    }
    return makeSpec(m_astreaLaunchProgram, arguments);
}

LaunchSpec LaunchService::windowsLaunch(const QString &path) const
{
    if (path.isEmpty()) {
        return {};
    }
    return makeSpec(m_windowsRunProgram, {QStringLiteral("--json"), path});
}

LaunchResult LaunchService::launch(const LaunchSpec &spec) const
{
    if (!spec.isValid()) {
        return {false, 0, QStringLiteral("invalid_launch_spec")};
    }

    const QFileInfo programInfo(spec.program);
    if (programInfo.isAbsolute() && !programInfo.isExecutable()) {
        return {false, 0, QStringLiteral("launcher_missing")};
    }

    qint64 processId = 0;
    if (!QProcess::startDetached(spec.program, spec.arguments, QString(), &processId)) {
        return {false, 0, QStringLiteral("launch_failed")};
    }
    return {true, processId, {}};
}

LaunchSpec LaunchService::makeSpec(
    const QString &program,
    const QStringList &arguments) const
{
    return {program, arguments};
}

} // namespace Astrea::Explorer::Native::Services
