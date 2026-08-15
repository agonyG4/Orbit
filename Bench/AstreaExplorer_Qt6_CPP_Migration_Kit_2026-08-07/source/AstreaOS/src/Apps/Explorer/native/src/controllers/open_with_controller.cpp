#include "controllers/open_with_controller.h"

#include <QFileInfo>
#include <QMimeDatabase>

#include <algorithm>

#include "services/desktop_file_id.h"

namespace Astrea::Explorer::Native::Backend {

OpenWithApplication OpenWithController::fromCatalogEntry(
    const Services::DesktopApplication &entry,
    bool isDefault)
{
    return {
        entry.id,
        entry.name,
        entry.icon,
        entry.desktopFile,
        isDefault,
        entry.mimeTypes,
    };
}

OpenWithController::OpenWithController(
    Services::LaunchService *launchService,
    QObject *parent)
    : QObject(parent)
    , m_launchService(launchService)
    , m_ownedMimeAppsService(std::make_unique<Services::MimeAppsService>())
    , m_mimeAppsService(m_ownedMimeAppsService.get())
    , m_ownedCatalog(std::make_unique<Services::DesktopApplicationCatalog>())
    , m_catalogService(m_ownedCatalog.get())
{
    m_mimeAppsService->setCatalog(m_catalogService);
}

OpenWithController::~OpenWithController() = default;

void OpenWithController::setMimeAppsService(Services::MimeAppsService *mimeAppsService)
{
    if (mimeAppsService == nullptr || mimeAppsService == m_mimeAppsService) {
        return;
    }
    m_ownedMimeAppsService.reset();
    m_mimeAppsService = mimeAppsService;
    m_mimeAppsService->setCatalog(m_catalogService);
}

void OpenWithController::setCatalog(Services::DesktopApplicationCatalog *catalog)
{
    if (catalog == nullptr || catalog == m_catalogService) {
        return;
    }
    m_ownedCatalog.reset();
    m_catalogService = catalog;
    if (m_mimeAppsService != nullptr) {
        m_mimeAppsService->setCatalog(m_catalogService);
    }
}

OpenWithController::DesktopCatalog OpenWithController::buildDesktopCatalog(
    const QStringList &roots)
{
    const auto snapshot = Services::DesktopApplicationCatalog::build(
        Services::XdgPaths::fromEnvironment(),
        roots);
    DesktopCatalog catalog;
    for (const auto &entry : snapshot) {
        catalog.insert(entry.id, fromCatalogEntry(entry));
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
    return applicationForId(m_selectedApplicationId);
}

OpenWithApplication OpenWithController::applicationForId(const QString &desktopId) const
{
    const auto direct = m_catalog.constFind(desktopId);
    if (direct != m_catalog.constEnd()) {
        return direct.value();
    }
    if (m_catalogService != nullptr && m_catalogService->ready()) {
        return fromCatalogEntry(
            Services::DesktopApplicationCatalog::resolve(
                desktopId,
                m_catalogService->snapshot()));
    }
    const QString normalized = Services::DesktopFileId::normalize(desktopId);
    const auto found = m_catalog.constFind(normalized);
    return found == m_catalog.constEnd() ? OpenWithApplication {} : found.value();
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
        return fromCatalogEntry(
            Services::DesktopApplicationCatalog::readDesktopEntry(
                candidate.absoluteFilePath(),
                Services::DesktopFileId::applicationRoots()));
    }
    const QString normalized = Services::DesktopFileId::normalize(desktopId);
    if (catalog != nullptr) {
        const auto found = catalog->constFind(normalized);
        if (found != catalog->constEnd()) {
            return found.value();
        }
    }
    const auto snapshot = Services::DesktopApplicationCatalog::build();
    return fromCatalogEntry(Services::DesktopApplicationCatalog::resolve(normalized, snapshot));
}

void OpenWithController::setApplications(
    const QVector<OpenWithApplication> &applicationsValue)
{
    m_applications = applicationsValue;
    if (!m_catalogReady) {
        m_catalog.clear();
        for (const OpenWithApplication &application : applicationsValue) {
            m_catalog.insert(application.id, application);
        }
    }
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

    const QString mime = QMimeDatabase().mimeTypeForFile(path).name();
    const auto applyCatalog = [this, mime, generation]() {
        if (generation != m_discoveryGeneration || m_catalogService == nullptr) {
            return;
        }
        m_catalog.clear();
        for (const auto &entry : m_catalogService->snapshot()) {
            m_catalog.insert(entry.id, fromCatalogEntry(entry));
        }
        m_catalogReady = true;

        const QStringList associated = m_mimeAppsService == nullptr
            ? QStringList {}
            : m_mimeAppsService->associationsForMime(mime);
        const QStringList defaults = m_mimeAppsService == nullptr
            ? QStringList {}
            : m_mimeAppsService->defaultsForMime(mime);
        const QString defaultId = defaults.value(0);
        QVector<OpenWithApplication> discovered;
        for (const QString &id : associated) {
            const auto found = m_catalog.constFind(id);
            if (found == m_catalog.constEnd()) {
                continue;
            }
            OpenWithApplication application = found.value();
            application.isDefault = application.id == defaultId;
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

    if (m_catalogService != nullptr && m_catalogService->ready()) {
        applyCatalog();
        return;
    }
    if (m_catalogService == nullptr) {
        m_busy = false;
        emit busyChanged();
        return;
    }
    connect(
        m_catalogService,
        &Services::DesktopApplicationCatalog::catalogReady,
        this,
        applyCatalog,
        Qt::SingleShotConnection);
    m_catalogService->discover();
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

} // namespace Astrea::Explorer::Native::Backend
