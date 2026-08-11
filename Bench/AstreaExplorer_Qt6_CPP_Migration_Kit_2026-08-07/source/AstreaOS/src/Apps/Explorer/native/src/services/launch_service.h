#pragma once

#include <QString>
#include <QStringList>

namespace Astrea::Explorer::Native::Services {

struct LaunchSpec
{
    QString program;
    QStringList arguments;

    bool isValid() const;
};

struct LaunchResult
{
    bool started = false;
    qint64 processId = 0;
    QString error;
};

class LaunchService final
{
public:
    LaunchService(QString astreaLaunchProgram, QString windowsRunProgram);

    LaunchSpec fileLaunch(const QString &path) const;
    LaunchSpec desktopLaunch(const QString &path) const;
    LaunchSpec desktopLaunch(const QString &desktopFile, const QString &targetPath) const;
    LaunchSpec windowsLaunch(const QString &path) const;
    LaunchResult launch(const LaunchSpec &spec) const;

private:
    LaunchSpec makeSpec(const QString &program, const QStringList &arguments) const;

    QString m_astreaLaunchProgram;
    QString m_windowsRunProgram;
};

} // namespace Astrea::Explorer::Native::Services
