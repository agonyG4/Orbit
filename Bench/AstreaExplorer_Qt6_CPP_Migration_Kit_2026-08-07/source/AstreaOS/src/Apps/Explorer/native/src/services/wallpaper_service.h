#pragma once

#include <QHash>
#include <QProcess>

namespace Astrea::Explorer::Native::Services {

class WallpaperService final : public QObject
{
    Q_OBJECT

public:
    explicit WallpaperService(QObject *parent = nullptr);
    quint64 apply(const QString &path);

signals:
    void finished(quint64 requestId, bool ok, const QString &error);

private:
    struct ActiveProcess
    {
        QProcess *process = nullptr;
    };

    QHash<quint64, ActiveProcess> m_active;
    quint64 m_nextRequest = 1;
};

} // namespace Astrea::Explorer::Native::Services
