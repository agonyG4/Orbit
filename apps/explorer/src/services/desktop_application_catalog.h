#pragma once

#include <QHash>
#include <QPointer>
#include <QThread>
#include <QObject>
#include <QString>
#include <QStringList>

#include "services/xdg_paths.h"

namespace Astrea::Explorer::Native::Services {

struct DesktopApplication
{
    QString id;
    QString name;
    QString icon;
    QString desktopFile;
    QString exec;
    QString type;
    QStringList mimeTypes;
    bool hidden = false;
    bool noDisplay = false;

    bool usable() const
    {
        return type == QStringLiteral("Application")
            && !hidden
            && !exec.trimmed().isEmpty();
    }
};

class DesktopApplicationCatalog final : public QObject
{
    Q_OBJECT

public:
    using Snapshot = QHash<QString, DesktopApplication>;

    explicit DesktopApplicationCatalog(
        XdgPaths paths = XdgPaths::fromEnvironment(),
        QObject *parent = nullptr);
    ~DesktopApplicationCatalog() override;

    const Snapshot &snapshot() const;
    bool ready() const;
    void discover();

    static Snapshot build(
        XdgPaths paths = XdgPaths::fromEnvironment(),
        const QStringList &roots = {});
    static DesktopApplication readDesktopEntry(
        const QString &desktopFile,
        const QStringList &applicationRoots = {});
    static DesktopApplication resolve(
        const QString &desktopId,
        const Snapshot &snapshot);

signals:
    void catalogReady();

private:
    XdgPaths m_paths;
    Snapshot m_snapshot;
    QPointer<QThread> m_discoveryThread;
    quint64 m_generation = 0;
    bool m_ready = false;
};

} // namespace Astrea::Explorer::Native::Services

Q_DECLARE_METATYPE(Astrea::Explorer::Native::Services::DesktopApplication)
Q_DECLARE_METATYPE(Astrea::Explorer::Native::Services::DesktopApplicationCatalog::Snapshot)
