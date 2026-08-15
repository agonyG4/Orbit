#include "services/desktop_application_catalog.h"

#include <QDir>
#include <QDirIterator>
#include <QFileInfo>
#include <QSettings>

#include <utility>

#include "services/desktop_file_id.h"

namespace Astrea::Explorer::Native::Services {

DesktopApplicationCatalog::DesktopApplicationCatalog(XdgPaths paths, QObject *parent)
    : QObject(parent)
    , m_paths(std::move(paths))
{
}

DesktopApplicationCatalog::~DesktopApplicationCatalog()
{
    if (m_discoveryThread != nullptr && m_discoveryThread->isRunning()) {
        m_discoveryThread->requestInterruption();
        m_discoveryThread->wait();
    }
}

const DesktopApplicationCatalog::Snapshot &DesktopApplicationCatalog::snapshot() const
{
    return m_snapshot;
}

bool DesktopApplicationCatalog::ready() const
{
    return m_ready;
}

void DesktopApplicationCatalog::discover()
{
    if (m_ready || (m_discoveryThread != nullptr && m_discoveryThread->isRunning())) {
        return;
    }
    const quint64 generation = ++m_generation;
    const XdgPaths paths = m_paths;
    const QPointer<DesktopApplicationCatalog> self(this);
    QThread *thread = QThread::create([self, paths, generation]() {
        if (!self || QThread::currentThread()->isInterruptionRequested()) {
            return;
        }
        const Snapshot snapshot = build(paths);
        if (!self || QThread::currentThread()->isInterruptionRequested()) {
            return;
        }
        QMetaObject::invokeMethod(
            self,
            [self, snapshot, generation]() {
                if (!self || generation != self->m_generation) {
                    return;
                }
                self->m_snapshot = snapshot;
                self->m_ready = true;
                emit self->catalogReady();
            },
            Qt::QueuedConnection);
    });
    m_discoveryThread = thread;
    connect(thread, &QThread::finished, this, [this, thread]() {
        if (m_discoveryThread == thread) {
            m_discoveryThread = nullptr;
        }
    });
    connect(thread, &QThread::finished, thread, &QObject::deleteLater);
    thread->start();
}

DesktopApplicationCatalog::Snapshot DesktopApplicationCatalog::build(
    XdgPaths paths,
    const QStringList &roots)
{
    const QStringList applicationRoots = roots.isEmpty() ? paths.applicationRoots() : roots;
    Snapshot catalog;
    for (const QString &root : applicationRoots) {
        if (root.trimmed().isEmpty()) {
            continue;
        }
        QDirIterator iterator(
            root,
            {QStringLiteral("*.desktop")},
            QDir::Files,
            QDirIterator::Subdirectories);
        while (iterator.hasNext()) {
            const QString desktopFile = iterator.next();
            const DesktopApplication application = readDesktopEntry(desktopFile, {root});
            if (application.usable()
                && !application.id.isEmpty()
                && !catalog.contains(application.id)) {
                catalog.insert(application.id, application);
            }
        }
    }
    return catalog;
}

DesktopApplication DesktopApplicationCatalog::readDesktopEntry(
    const QString &desktopFile,
    const QStringList &applicationRoots)
{
    const QFileInfo info(desktopFile);
    if (!info.isFile()) {
        return {};
    }
    QSettings settings(info.absoluteFilePath(), QSettings::IniFormat);
    settings.beginGroup(QStringLiteral("Desktop Entry"));
    DesktopApplication application;
    application.type = settings.value(QStringLiteral("Type")).toString();
    application.exec = settings.value(QStringLiteral("Exec")).toString();
    application.hidden = settings.value(QStringLiteral("Hidden"), false).toBool();
    application.noDisplay = settings.value(QStringLiteral("NoDisplay"), false).toBool();
    application.name = settings.value(QStringLiteral("Name")).toString();
    application.icon = settings.value(QStringLiteral("Icon")).toString();
    application.mimeTypes = settings.value(QStringLiteral("MimeType"))
                               .toString()
                               .split(QLatin1Char(';'), Qt::SkipEmptyParts);
    settings.endGroup();
    application.desktopFile = info.absoluteFilePath();
    application.id = DesktopFileId::fromPath(
        application.desktopFile,
        applicationRoots);
    if (application.name.isEmpty()) {
        application.name = application.id;
    }
    return application;
}

DesktopApplication DesktopApplicationCatalog::resolve(
    const QString &desktopId,
    const Snapshot &snapshot)
{
    const QFileInfo candidate(desktopId);
    if (candidate.isFile()) {
        const QString absolutePath = candidate.absoluteFilePath();
        for (const DesktopApplication &entry : snapshot) {
            if (entry.desktopFile == absolutePath) {
                return entry;
            }
        }
        return readDesktopEntry(absolutePath);
    }
    const QString normalized = DesktopFileId::normalize(desktopId);
    return snapshot.value(normalized);
}

} // namespace Astrea::Explorer::Native::Services
