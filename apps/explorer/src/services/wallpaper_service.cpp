#include "services/wallpaper_service.h"

#include <QStandardPaths>

namespace Astrea::Explorer::Native::Services {

WallpaperService::WallpaperService(QObject *parent)
    : QObject(parent)
{
}

quint64 WallpaperService::apply(const QString &path)
{
    const quint64 requestId = m_nextRequest++;
    const QString program = !QStandardPaths::findExecutable(QStringLiteral("awww")).isEmpty()
        ? QStringLiteral("awww")
        : QStringLiteral("swww");
    const QString resolved = QStandardPaths::findExecutable(program);
    if (resolved.isEmpty()) {
        emit finished(requestId, false, QStringLiteral("no supported wallpaper command is installed"));
        return requestId;
    }
    auto *process = new QProcess(this);
    process->setProgram(resolved);
    process->setArguments({QStringLiteral("img"), path});
    process->setProcessChannelMode(QProcess::SeparateChannels);
    m_active.insert(requestId, {process});
    connect(process, &QProcess::errorOccurred, this, [this, requestId](QProcess::ProcessError) {
        auto it = m_active.find(requestId);
        if (it == m_active.end()) return;
        const QString error = it->process->errorString();
        it->process->deleteLater();
        m_active.erase(it);
        emit finished(requestId, false, error);
    });
    connect(process, &QProcess::finished, this, [this, requestId](int exitCode, QProcess::ExitStatus status) {
        auto it = m_active.find(requestId);
        if (it == m_active.end()) return;
        const QString error = QString::fromUtf8(it->process->readAllStandardError()).trimmed();
        const bool ok = status == QProcess::NormalExit && exitCode == 0;
        it->process->deleteLater();
        m_active.erase(it);
        emit finished(requestId, ok, ok ? QString() : (error.isEmpty() ? QStringLiteral("wallpaper command failed") : error));
    });
    process->start();
    return requestId;
}

} // namespace Astrea::Explorer::Native::Services
