#include "controllers/explorer_settings_controller.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonValue>
#include <QtGlobal>

#include <algorithm>

#include "controllers/device_controller.h"
#include "controllers/navigation_controller.h"

namespace Astrea::Explorer::Native::Backend {

namespace {

QString canonicalAutoMountDeviceIdsJson(const QString &json)
{
    const QJsonDocument document = QJsonDocument::fromJson(json.toUtf8());
    if (!document.isArray()) {
        return QStringLiteral("[]");
    }

    QStringList ids;
    for (const QJsonValue &value : document.array()) {
        if (value.isString() && !value.toString().isEmpty()) {
            ids.append(value.toString());
        }
    }
    std::sort(ids.begin(), ids.end(), [](const QString &left, const QString &right) {
        return left < right;
    });
    ids.removeDuplicates();

    QJsonArray values;
    for (const QString &id : ids) {
        values.append(id);
    }
    return QString::fromUtf8(QJsonDocument(values).toJson(QJsonDocument::Compact));
}

} // namespace

ExplorerSettingsController::ExplorerSettingsController(
    Services::SettingsService *settingsService,
    QObject *parent)
    : QObject(parent)
    , m_settingsService(settingsService)
{
    if (m_settingsService != nullptr) {
        m_settings = m_settingsService->load();
        const QString canonical = canonicalAutoMountDeviceIdsJson(
            m_settings.autoMountDeviceIdsJson);
        if (m_settings.autoMountDeviceIdsJson != canonical) {
            m_settings.autoMountDeviceIdsJson = canonical;
            m_settingsService->save(m_settings);
        }
    }
}

const Services::ExplorerSettings &ExplorerSettingsController::settings() const
{
    return m_settings;
}

bool ExplorerSettingsController::showPreview() const
{
    return m_settings.showPreview;
}

QString ExplorerSettingsController::viewMode() const
{
    return m_settings.viewMode;
}

QString ExplorerSettingsController::sortField() const
{
    return m_settings.sortField;
}

bool ExplorerSettingsController::sortAscending() const
{
    return m_settings.sortAscending;
}

bool ExplorerSettingsController::showHidden() const
{
    return m_settings.showHidden;
}

bool ExplorerSettingsController::foldersFirst() const
{
    return m_settings.foldersFirst;
}

bool ExplorerSettingsController::groupingEnabled() const
{
    return m_settings.groupingEnabled;
}

double ExplorerSettingsController::zoomLevel() const
{
    return m_settings.zoomLevel;
}

QString ExplorerSettingsController::currentPath() const
{
    return m_settings.currentPath;
}

QString ExplorerSettingsController::autoMountDeviceIdsJson() const
{
    return m_devices == nullptr
        ? m_settings.autoMountDeviceIdsJson
        : m_devices->autoMountDeviceIdsJson();
}

QString ExplorerSettingsController::sidebarFavoritesJson() const
{
    return m_settings.sidebarFavoritesJson;
}

QString ExplorerSettingsController::sidebarHiddenDefaultFavoritesJson() const
{
    return m_settings.sidebarHiddenDefaultFavoritesJson;
}

void ExplorerSettingsController::bindNavigation(NavigationController *navigation)
{
    if (m_navigation == navigation) {
        return;
    }
    m_navigation = navigation;
    if (m_navigation == nullptr) {
        return;
    }

    m_navigation->setShowHidden(m_settings.showHidden);
    m_navigation->setSortField(m_settings.sortField);
    m_navigation->setSortAscending(m_settings.sortAscending);
    m_navigation->setFoldersFirst(m_settings.foldersFirst);
    m_navigation->setPreviews(m_settings.showPreview);

    connect(
        m_navigation,
        &NavigationController::listingOptionsChanged,
        this,
        &ExplorerSettingsController::handleListingOptionsChanged);
    connect(
        m_navigation,
        &NavigationController::currentPathChanged,
        this,
        &ExplorerSettingsController::handleCurrentPathChanged);
}

void ExplorerSettingsController::bindDeviceController(DeviceController *devices)
{
    if (m_devices == devices) {
        return;
    }
    m_devices = devices;
    if (m_devices == nullptr) {
        return;
    }

    connect(
        m_devices,
        &DeviceController::autoMountChanged,
        this,
        &ExplorerSettingsController::handleAutoMountChanged);
    m_devices->setAutoMountDeviceIdsJson(m_settings.autoMountDeviceIdsJson);
    handleAutoMountChanged();
}

void ExplorerSettingsController::setShowPreview(bool value)
{
    if (m_settings.showPreview == value) {
        return;
    }
    m_settings.showPreview = value;
    persist();
    if (m_navigation != nullptr) {
        m_navigation->setPreviews(value);
    }
    emit showPreviewChanged();
}

void ExplorerSettingsController::setViewMode(const QString &value)
{
    if (m_settings.viewMode == value) {
        return;
    }
    m_settings.viewMode = value;
    persist();
    emit viewModeChanged();
}

void ExplorerSettingsController::setSortField(const QString &value)
{
    if (m_navigation != nullptr) {
        m_navigation->setSortField(value);
        return;
    }
    if (m_settings.sortField == value) {
        return;
    }
    m_settings.sortField = value;
    persist();
    emit sortFieldChanged();
}

void ExplorerSettingsController::setSortAscending(bool value)
{
    if (m_navigation != nullptr) {
        m_navigation->setSortAscending(value);
        return;
    }
    if (m_settings.sortAscending == value) {
        return;
    }
    m_settings.sortAscending = value;
    persist();
    emit sortAscendingChanged();
}

void ExplorerSettingsController::setShowHidden(bool value)
{
    if (m_navigation != nullptr) {
        m_navigation->setShowHidden(value);
        return;
    }
    if (m_settings.showHidden == value) {
        return;
    }
    m_settings.showHidden = value;
    persist();
    emit showHiddenChanged();
}

void ExplorerSettingsController::setFoldersFirst(bool value)
{
    if (m_navigation != nullptr) {
        m_navigation->setFoldersFirst(value);
        return;
    }
    if (m_settings.foldersFirst == value) {
        return;
    }
    m_settings.foldersFirst = value;
    persist();
    emit foldersFirstChanged();
}

void ExplorerSettingsController::setGroupingEnabled(bool value)
{
    if (m_settings.groupingEnabled == value) {
        return;
    }
    m_settings.groupingEnabled = value;
    persist();
    emit groupingEnabledChanged();
}

void ExplorerSettingsController::setZoomLevel(double value)
{
    const double clamped = qBound(0.75, value, 2.0);
    if (qFuzzyCompare(m_settings.zoomLevel, clamped)) {
        return;
    }
    m_settings.zoomLevel = clamped;
    persist();
    emit zoomLevelChanged();
}

void ExplorerSettingsController::setAutoMountDeviceIdsJson(const QString &json)
{
    if (m_devices != nullptr) {
        m_devices->setAutoMountDeviceIdsJson(json);
        handleAutoMountChanged();
        return;
    }
    const QString canonical = canonicalAutoMountDeviceIdsJson(json);
    if (m_settings.autoMountDeviceIdsJson == canonical) {
        return;
    }
    m_settings.autoMountDeviceIdsJson = canonical;
    persist();
    emit autoMountDeviceIdsJsonChanged();
}

void ExplorerSettingsController::setSidebarFavoritesJson(const QString &json)
{
    if (m_settings.sidebarFavoritesJson == json) {
        return;
    }
    m_settings.sidebarFavoritesJson = json;
    persist();
    emit sidebarFavoritesJsonChanged();
}

void ExplorerSettingsController::setSidebarHiddenDefaultFavoritesJson(const QString &json)
{
    if (m_settings.sidebarHiddenDefaultFavoritesJson == json) {
        return;
    }
    m_settings.sidebarHiddenDefaultFavoritesJson = json;
    persist();
    emit sidebarHiddenDefaultFavoritesJsonChanged();
}

void ExplorerSettingsController::handleListingOptionsChanged()
{
    if (m_navigation == nullptr) {
        return;
    }

    const bool previewChanged = m_settings.showPreview != m_navigation->previews();
    const bool sortFieldValueChanged = m_settings.sortField != m_navigation->sortField();
    const bool sortAscendingValueChanged = m_settings.sortAscending != m_navigation->sortAscending();
    const bool showHiddenValueChanged = m_settings.showHidden != m_navigation->showHidden();
    const bool foldersFirstValueChanged = m_settings.foldersFirst != m_navigation->foldersFirst();
    if (!previewChanged && !sortFieldValueChanged && !sortAscendingValueChanged
        && !showHiddenValueChanged && !foldersFirstValueChanged) {
        return;
    }

    m_settings.showPreview = m_navigation->previews();
    m_settings.sortField = m_navigation->sortField();
    m_settings.sortAscending = m_navigation->sortAscending();
    m_settings.showHidden = m_navigation->showHidden();
    m_settings.foldersFirst = m_navigation->foldersFirst();
    persist();
    if (previewChanged) {
        emit showPreviewChanged();
    }
    if (sortFieldValueChanged) {
        emit sortFieldChanged();
    }
    if (sortAscendingValueChanged) {
        emit sortAscendingChanged();
    }
    if (showHiddenValueChanged) {
        emit showHiddenChanged();
    }
    if (foldersFirstValueChanged) {
        emit foldersFirstChanged();
    }
}

void ExplorerSettingsController::handleCurrentPathChanged()
{
    if (m_navigation == nullptr || m_settings.currentPath == m_navigation->currentPath()) {
        return;
    }
    m_settings.currentPath = m_navigation->currentPath();
    persist();
    emit currentPathChanged();
}

void ExplorerSettingsController::handleAutoMountChanged()
{
    if (m_devices == nullptr
        || m_settings.autoMountDeviceIdsJson == m_devices->autoMountDeviceIdsJson()) {
        return;
    }
    m_settings.autoMountDeviceIdsJson = m_devices->autoMountDeviceIdsJson();
    persist();
    emit autoMountDeviceIdsJsonChanged();
}

void ExplorerSettingsController::persist()
{
    if (m_settingsService != nullptr) {
        m_settingsService->save(m_settings);
    }
}

} // namespace Astrea::Explorer::Native::Backend
