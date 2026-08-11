#include "services/launch_service.h"

#include <utility>

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
    if (desktopFile.isEmpty() || targetPath.isEmpty()) {
        return {};
    }
    return makeSpec(
        m_astreaLaunchProgram,
        {QStringLiteral("--desktop"), desktopFile, QStringLiteral("--file"), targetPath});
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
