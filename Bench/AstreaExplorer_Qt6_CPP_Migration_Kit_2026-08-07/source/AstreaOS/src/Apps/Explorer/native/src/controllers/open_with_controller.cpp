#include "controllers/open_with_controller.h"

#include <QDir>
#include <QDirIterator>
#include <QFileInfo>
#include <QMimeDatabase>
#include <QSettings>
#include <QStandardPaths>

#include <algorithm>

#include "services/desktop_file_id.h"

namespace Astrea::Explorer::Native::Backend {

namespace {

OpenWithApplication readDesktopEntry(
    const QString &desktopFile,
    const QStringList &applicationRoots = {})
{
    const QFileInfo info(desktopFile);
    if (!info.isFile()) {
        return {};
    }

    QSettings settings(info.absoluteFilePath(), QSettings::IniFormat);
    settings.beginGroup(QStringLiteral("Desktop Entry"));
    const QString type = settings.value(QStringLiteral("Type")).toString();
    const QString exec = settings.value(QStringLiteral("Exec")).toString();
    if (type != QStringLiteral("Application")
        || exec.isEmpty()
        || settings.value(QStringLiteral("Hidden"), false).toBool()) {
        settings.endGroup();
        return {};
    }

    const QString id = Services::DesktopFileId::fromPath(
        info.absoluteFilePath(),
        applicationRoots);
    const QStringList mimeTypes = settings.value(QStringLiteral("MimeType"))
                                      .toString()
                                      .split(QLatin1Char(';'), Qt::SkipEmptyParts);
    const OpenWithApplication application {
        id,
        settings.value(QStringLiteral("Name"), id).toString(),
        settings.value(QStringLiteral("Icon")).toString(),
        info.absoluteFilePath(),
        false,
        mimeTypes,
    };
    settings.endGroup();
    return application;
}

} // namespace

OpenWithController::OpenWithController(
    Services::LaunchService *launchService,
    QObject *parent)
    : QObject(parent)
    , m_launchService(launchService)
    , m_ownedMimeAppsService(std::make_unique<Services::MimeAppsService>())
    , m_mimeAppsService(m_ownedMimeAppsService.get())
{
}

OpenWithController::~OpenWithController()
{
    if (m_discoveryThread != nullptr && m_discoveryThread->isRunning()) {
        m_discoveryThread->requestInterruption();
        m_discoveryThread->wait();
    }
}

void OpenWithController::setMimeAppsService(Services::MimeAppsService *mimeAppsService)
{
    if (mimeAppsService == nullptr || mimeAppsService == m_mimeAppsService) {
        return;
    }
    m_ownedMimeAppsService.reset();
    m_mimeAppsService = mimeAppsService;
}

OpenWithController::DesktopCatalog OpenWithController::buildDesktopCatalog(
    const QStringList &roots)
{
    const QStringList applicationRoots = roots.isEmpty()
        ? Services::DesktopFileId::applicationRoots()
        : roots;
    DesktopCatalog catalog;
    for (const QString &root : applicationRoots) {
        if (root.isEmpty()) {
            continue;
        }
        QDirIterator iterator(root, {QStringLiteral("*.desktop")}, QDir::Files,
                              QDirIterator::Subdirectories);
        while (iterator.hasNext()) {
            const OpenWithApplication application = readDesktopEntry(
                iterator.next(),
                {root});
            if (!application.desktopFile.isEmpty()
                && !catalog.contains(application.id)) {
                catalog.insert(application.id, application);
            }
        }
    }
    return catalog;
}

QVector<OpenWithApplication> OpenWithController::applications() const
{
    return m_applications;
}

QVariantList OpenWithController::applicationList() const
{
    QVariantList values;
    values.reserve(m_applications.size());
    for (const OpenWithApplication &application : m_applications) {
        QVariantMap value;
        value.insert(QStringLiteral("id"), application.id);
        value.insert(QStringLiteral("name"), application.name);
        value.insert(QStringLiteral("icon"), application.icon);
        value.insert(QStringLiteral("desktopFile"), application.desktopFile);
        value.insert(QStringLiteral("isDefault"), application.isDefault);
        values.append(value);
    }
    return values;
}

QString OpenWithController::selectedApplicationId() const
{
    return m_selectedApplicationId;
}

OpenWithApplication OpenWithController::selectedApplication() const
{
    for (const OpenWithApplication &application : m_applications) {
        if (application.id == m_selectedApplicationId) {
            return application;
        }
    }
    return {};
}

bool OpenWithController::busy() const
{
    return m_busy;
}

QString OpenWithController::error() const
{
    return m_error;
}

OpenWithApplication OpenWithController::resolveDesktopEntry(
    const QString &desktopId,
    const DesktopCatalog *catalog)
{
    const QFileInfo candidate(desktopId);
    if (candidate.isFile()) {
        return readDesktopEntry(
            candidate.absoluteFilePath(),
            Services::DesktopFileId::applicationRoots());
    }

    QString fileName = desktopId.trimmed();
    if (fileName.isEmpty()) {
        return {};
    }
    fileName = Services::DesktopFileId::normalize(fileName);

    if (catalog != nullptr) {
        const auto found = catalog->constFind(fileName);
        if (found != catalog->constEnd()) {
            return found.value();
        }
    }

    const QStringList applicationRoots = Services::DesktopFileId::applicationRoots();
    for (const QString &root : applicationRoots) {
        if (root.isEmpty()) {
            continue;
        }
        const QString directPath = QDir(root).filePath(fileName);
        if (QFileInfo(directPath).isFile()) {
            return readDesktopEntry(directPath, {root});
        }
        QDirIterator iterator(root, {QStringLiteral("*.desktop")}, QDir::Files,
                              QDirIterator::Subdirectories);
        while (iterator.hasNext()) {
            const QString desktopFile = iterator.next();
            if (Services::DesktopFileId::fromPath(desktopFile, {root}) == fileName) {
                return readDesktopEntry(desktopFile, {root});
            }
        }
    }
    return {};
}

void OpenWithController::setApplications(
    const QVector<OpenWithApplication> &applicationsValue)
{
    m_applications = applicationsValue;
    m_selectedApplicationId.clear();
    for (const OpenWithApplication &application : m_applications) {
        if (application.isDefault || m_selectedApplicationId.isEmpty()) {
            m_selectedApplicationId = application.id;
        }
    }
    emit applicationsChanged();
    emit selectionChanged();
}

void OpenWithController::discover(const QString &path)
{
    const quint64 generation = ++m_discoveryGeneration;
    m_busy = true;
    m_error.clear();
    emit busyChanged();
    emit errorChanged();

    const QMimeType mimeType = QMimeDatabase().mimeTypeForFile(path);
    const QStringList applicationRoots = QStandardPaths::standardLocations(
        QStandardPaths::ApplicationsLocation);
    const QString mime = mimeType.name();

    const auto applyCatalog = [this, path, mime, generation](const DesktopCatalog &catalog) {
        if (generation != m_discoveryGeneration) {
            return;
        }

        m_catalog = catalog;
        m_catalogReady = true;
        QVector<OpenWithApplication> discovered;
        discovered.reserve(m_catalog.size());
        const QString defaultId = m_mimeAppsService == nullptr
            ? QString()
            : Services::DesktopFileId::normalize(
                  m_mimeAppsService->defaultsForMime(mime).value(0));

        for (const OpenWithApplication &candidate : m_catalog) {
            if (!mimeMatches(candidate.mimeTypes, mime)) {
                continue;
            }
            OpenWithApplication application = candidate;
            application.isDefault = !defaultId.isEmpty() && application.id == defaultId;
            discovered.append(application);
        }
        std::sort(discovered.begin(), discovered.end(), [](const auto &left, const auto &right) {
            const int nameOrder = QString::compare(left.name, right.name, Qt::CaseInsensitive);
            return nameOrder == 0 ? left.id < right.id : nameOrder < 0;
        });

        setApplications(discovered);
        m_busy = false;
        emit busyChanged();
    };

    if (m_catalogReady) {
        applyCatalog(m_catalog);
        return;
    }

    const QPointer<OpenWithController> self(this);
    QThread *thread = QThread::create([self, applicationRoots, applyCatalog, generation]() mutable {
        if (!self || QThread::currentThread()->isInterruptionRequested()) {
            return;
        }
        const DesktopCatalog catalog = buildDesktopCatalog(applicationRoots);
        if (!self || QThread::currentThread()->isInterruptionRequested()) {
            return;
        }
        QMetaObject::invokeMethod(
            self,
            [self, catalog, applyCatalog, generation]() {
                if (self) {
                    applyCatalog(catalog);
                }
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

void OpenWithController::selectApplication(const QString &id)
{
    if (m_selectedApplicationId == id) {
        return;
    }
    for (const OpenWithApplication &application : m_applications) {
        if (application.id == id) {
            m_selectedApplicationId = id;
            emit selectionChanged();
            return;
        }
    }
}

bool OpenWithController::launchSelected(const QString &path)
{
    const OpenWithApplication application = selectedApplication();
    if (application.desktopFile.isEmpty() || m_launchService == nullptr) {
        m_error = QStringLiteral("application_not_selected");
        emit errorChanged();
        return false;
    }
    const Services::LaunchResult result = m_launchService->launch(
        m_launchService->desktopLaunch(application.desktopFile, path));
    if (!result.started) {
        m_error = result.error;
        emit errorChanged();
        return false;
    }
    emit launched(application.id);
    return true;
}

bool OpenWithController::mimeMatches(
    const QStringList &mimeTypes,
    const QString &mime)
{
    for (const QString &candidate : mimeTypes) {
        if (candidate == mime || (candidate.endsWith(QStringLiteral("/*"))
                                  && mime.startsWith(candidate.left(candidate.size() - 1)))) {
            return true;
        }
    }
    return false;
}

} // namespace Astrea::Explorer::Native::Backend
