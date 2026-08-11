#include "controllers/open_with_controller.h"

#include <QDir>
#include <QDirIterator>
#include <QFileInfo>
#include <QMimeDatabase>
#include <QSettings>
#include <QStandardPaths>

namespace Astrea::Explorer::Native::Backend {

namespace {

OpenWithApplication readDesktopEntry(const QString &desktopFile)
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

    const QString id = info.completeBaseName();
    const OpenWithApplication application {
        id,
        settings.value(QStringLiteral("Name"), id).toString(),
        settings.value(QStringLiteral("Icon")).toString(),
        info.absoluteFilePath(),
        false,
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
{
}

OpenWithController::DesktopCatalog OpenWithController::buildDesktopCatalog(
    const QStringList &roots)
{
    const QStringList applicationRoots = roots.isEmpty()
        ? QStandardPaths::standardLocations(QStandardPaths::ApplicationsLocation)
        : roots;
    DesktopCatalog catalog;
    for (const QString &root : applicationRoots) {
        if (root.isEmpty()) {
            continue;
        }
        QDirIterator iterator(root, {QStringLiteral("*.desktop")}, QDir::Files,
                              QDirIterator::Subdirectories);
        while (iterator.hasNext()) {
            const OpenWithApplication application = readDesktopEntry(iterator.next());
            if (!application.desktopFile.isEmpty()) {
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
        return readDesktopEntry(candidate.absoluteFilePath());
    }

    QString fileName = desktopId.trimmed();
    if (fileName.isEmpty()) {
        return {};
    }
    if (!fileName.endsWith(QStringLiteral(".desktop"))) {
        fileName += QStringLiteral(".desktop");
    }

    if (catalog != nullptr) {
        const QString id = QFileInfo(fileName).completeBaseName();
        const auto found = catalog->constFind(id);
        if (found != catalog->constEnd()) {
            return found.value();
        }
    }

    const QStringList applicationRoots = QStandardPaths::standardLocations(
        QStandardPaths::ApplicationsLocation);
    for (const QString &root : applicationRoots) {
        if (root.isEmpty()) {
            continue;
        }
        QDirIterator iterator(root, {QStringLiteral("*.desktop")}, QDir::Files,
                              QDirIterator::Subdirectories);
        while (iterator.hasNext()) {
            const QString desktopFile = iterator.next();
            if (QFileInfo(desktopFile).fileName() == fileName) {
                return readDesktopEntry(desktopFile);
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
    m_busy = true;
    m_error.clear();
    emit busyChanged();
    emit errorChanged();

    const QMimeType mimeType = QMimeDatabase().mimeTypeForFile(path);
    QVector<OpenWithApplication> discovered;
    const QStringList applicationRoots = QStandardPaths::standardLocations(
        QStandardPaths::ApplicationsLocation);
    for (const QString &root : applicationRoots) {
        if (root.isEmpty()) {
            continue;
        }
        QDirIterator iterator(root, {QStringLiteral("*.desktop")}, QDir::Files,
                              QDirIterator::Subdirectories);
        while (iterator.hasNext()) {
            const QString desktopFile = iterator.next();
            const OpenWithApplication application = resolveDesktopEntry(desktopFile);
            if (application.desktopFile.isEmpty()) {
                continue;
            }
            QSettings settings(desktopFile, QSettings::IniFormat);
            settings.beginGroup(QStringLiteral("Desktop Entry"));
            if (settings.value(QStringLiteral("Type")).toString() != QStringLiteral("Application")
                || settings.value(QStringLiteral("Hidden"), false).toBool()) {
                settings.endGroup();
                continue;
            }
            const QStringList mimeTypes = settings.value(QStringLiteral("MimeType"))
                                              .toString()
                                              .split(QLatin1Char(';'), Qt::SkipEmptyParts);
            if (!mimeMatches(mimeTypes, mimeType.name())) {
                settings.endGroup();
                continue;
            }
            discovered.append(application);
            settings.endGroup();
        }
    }
    setApplications(discovered);
    m_busy = false;
    emit busyChanged();
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
    const Services::LaunchResult result =
        m_launchService->launch(m_launchService->desktopLaunch(application.desktopFile));
    if (!result.started) {
        m_error = result.error;
        emit errorChanged();
        return false;
    }
    Q_UNUSED(path);
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
