/****************************************************************************
** Meta object code from reading C++ file 'app_state_facade.h'
**
** Created by: The Qt Meta Object Compiler version 69 (Qt 6.11.1)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../../../native/src/controllers/app_state_facade.h"
#include <QtCore/qmetatype.h>

#include <QtCore/qtmochelpers.h>

#include <memory>


#include <QtCore/qxptype_traits.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'app_state_facade.h' doesn't include <QObject>."
#elif Q_MOC_OUTPUT_REVISION != 69
#error "This file was generated using the moc from 6.11.1. It"
#error "cannot be used with the include files from this version of Qt."
#error "(The moc has changed too much.)"
#endif

#ifndef Q_CONSTINIT
#define Q_CONSTINIT
#endif

QT_WARNING_PUSH
QT_WARNING_DISABLE_DEPRECATED
QT_WARNING_DISABLE_GCC("-Wuseless-cast")
namespace {
struct qt_meta_tag_ZN6Astrea8Explorer6Native7Backend14AppStateFacadeE_t {};
} // unnamed namespace

template <> constexpr inline auto Astrea::Explorer::Native::Backend::AppStateFacade::qt_create_metaobjectdata<qt_meta_tag_ZN6Astrea8Explorer6Native7Backend14AppStateFacadeE_t>()
{
    namespace QMC = QtMocConstants;
    QtMocHelpers::StringRefStorage qt_stringData {
        "Astrea::Explorer::Native::Backend::AppStateFacade",
        "currentPathChanged",
        "",
        "historyChanged",
        "tabsChanged",
        "activeTabIndexChanged",
        "loadingDirChanged",
        "loadErrorChanged",
        "remoteDirectoryActiveChanged",
        "searchStateChanged",
        "selectedFileChanged",
        "selectedFilesChanged",
        "lastSelectedIndexChanged",
        "fileModelRevisionChanged",
        "showPreviewChanged",
        "viewModeChanged",
        "sortFieldChanged",
        "sortAscChanged",
        "showHiddenChanged",
        "foldersFirstChanged",
        "groupingEnabledChanged",
        "zoomLevelChanged",
        "autoMountDeviceIdsJsonChanged",
        "sidebarFavoritesJsonChanged",
        "sidebarHiddenDefaultFavoritesJsonChanged",
        "sidebarFavoritesChanged",
        "dialogStateChanged",
        "contextMenuOpening",
        "owner",
        "clipboardStateChanged",
        "fileOperationStateChanged",
        "pasteConflictStateChanged",
        "deviceStateChanged",
        "handleModelChanged",
        "handleListingOptionsChanged",
        "persistCurrentPath",
        "navigateTo",
        "BackendRequestId",
        "path",
        "submitSearch",
        "root",
        "query",
        "startSearch",
        "hideSearch",
        "clearSearch",
        "goBack",
        "goForward",
        "createTab",
        "closeTab",
        "index",
        "switchTab",
        "closeTabById",
        "tabId",
        "switchTabById",
        "tabIndexById",
        "moveTab",
        "fromIndex",
        "toIndex",
        "refreshCurrentFolder",
        "replaceFileModel",
        "QVariantList",
        "items",
        "updateFileModelMetadata",
        "removePathsFromFileModel",
        "paths",
        "increaseZoom",
        "decreaseZoom",
        "resetZoom",
        "setZoom",
        "level",
        "selectedItem",
        "QVariant",
        "fileUrlForPath",
        "joinPath",
        "directory",
        "fileName",
        "selectedPathsInCurrentFolder",
        "selectedUriListInCurrentFolder",
        "fileMatchesDialogFilter",
        "isDirectory",
        "dropFilePaths",
        "destination",
        "mode",
        "dropFiles",
        "urls",
        "copySelected",
        "cutSelected",
        "pasteFiles",
        "isCutPending",
        "name",
        "resolvePasteConflict",
        "policy",
        "renamePasteConflict",
        "cancelPasteConflict",
        "requestMountDevice",
        "devicePath",
        "fromAutoMount",
        "openAfterMount",
        "requestUnmountDevice",
        "mountPath",
        "requestRemountDevice",
        "toggleDeviceAutoMount",
        "deviceId",
        "enabled",
        "isRecentPath",
        "isTrashPath",
        "canPinSidebarFavorite",
        "isSidebarFavorite",
        "visibleDefaultSidebarFavorites",
        "pinSidebarFavorite",
        "label",
        "icon",
        "removeSidebarFavorite",
        "announceContextMenuOpening",
        "isSelected",
        "clearSelection",
        "selectAll",
        "selectByName",
        "handleSelection",
        "ctrlMode",
        "shiftMode",
        "preserveCurrentSelection",
        "fileModel",
        "QAbstractItemModel*",
        "homePath",
        "runtimeRoot",
        "backendPath",
        "helperPath",
        "wallpaperManagerPath",
        "astreaLaunch",
        "windowsRun",
        "networkRootPath",
        "trashFilesPath",
        "trashInfoPath",
        "recentVirtualPath",
        "isPortalDialog",
        "currentPath",
        "history",
        "historyIdx",
        "tabs",
        "breadcrumbParts",
        "activeTabIndex",
        "loadingDir",
        "loadError",
        "remoteDirectoryActive",
        "searchActive",
        "searchVisible",
        "searchQuery",
        "selectedFile",
        "selectedFiles",
        "lastSelectedIndex",
        "fileModelRevision",
        "fileModelFilling",
        "showPreview",
        "previewsEnabled",
        "viewMode",
        "sortField",
        "sortAsc",
        "showHidden",
        "foldersFirst",
        "groupingEnabled",
        "zoomLevel",
        "autoMountDeviceIdsJson",
        "sidebarFavoritesJson",
        "sidebarHiddenDefaultFavoritesJson",
        "sidebarFavorites",
        "sidebarHiddenDefaultFavorites",
        "sidebarFavoritesRevision",
        "defaultSidebarFavoritePaths",
        "inTrashView",
        "dialogActive",
        "dialogMode",
        "dialogFilePatterns",
        "clipboardFiles",
        "clipboardMode",
        "fileOperationRunning",
        "fileOperationProgress",
        "fileOperationPercent",
        "fileOperationFileName",
        "fileOperationStatus",
        "fileOperationError",
        "fileOperationDestination",
        "fileOperationDoneCount",
        "fileOperationTotalCount",
        "fileOperationMode",
        "pasteConflictVisible",
        "pasteConflictItems",
        "pendingPasteRename",
        "deviceModel",
        "deviceError"
    };

    QtMocHelpers::UintData qt_methods {
        // Signal 'currentPathChanged'
        QtMocHelpers::SignalData<void()>(1, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'historyChanged'
        QtMocHelpers::SignalData<void()>(3, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'tabsChanged'
        QtMocHelpers::SignalData<void()>(4, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'activeTabIndexChanged'
        QtMocHelpers::SignalData<void()>(5, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'loadingDirChanged'
        QtMocHelpers::SignalData<void()>(6, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'loadErrorChanged'
        QtMocHelpers::SignalData<void()>(7, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'remoteDirectoryActiveChanged'
        QtMocHelpers::SignalData<void()>(8, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'searchStateChanged'
        QtMocHelpers::SignalData<void()>(9, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'selectedFileChanged'
        QtMocHelpers::SignalData<void()>(10, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'selectedFilesChanged'
        QtMocHelpers::SignalData<void()>(11, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'lastSelectedIndexChanged'
        QtMocHelpers::SignalData<void()>(12, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'fileModelRevisionChanged'
        QtMocHelpers::SignalData<void()>(13, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'showPreviewChanged'
        QtMocHelpers::SignalData<void()>(14, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'viewModeChanged'
        QtMocHelpers::SignalData<void()>(15, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'sortFieldChanged'
        QtMocHelpers::SignalData<void()>(16, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'sortAscChanged'
        QtMocHelpers::SignalData<void()>(17, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'showHiddenChanged'
        QtMocHelpers::SignalData<void()>(18, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'foldersFirstChanged'
        QtMocHelpers::SignalData<void()>(19, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'groupingEnabledChanged'
        QtMocHelpers::SignalData<void()>(20, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'zoomLevelChanged'
        QtMocHelpers::SignalData<void()>(21, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'autoMountDeviceIdsJsonChanged'
        QtMocHelpers::SignalData<void()>(22, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'sidebarFavoritesJsonChanged'
        QtMocHelpers::SignalData<void()>(23, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'sidebarHiddenDefaultFavoritesJsonChanged'
        QtMocHelpers::SignalData<void()>(24, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'sidebarFavoritesChanged'
        QtMocHelpers::SignalData<void()>(25, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'dialogStateChanged'
        QtMocHelpers::SignalData<void()>(26, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'contextMenuOpening'
        QtMocHelpers::SignalData<void(const QString &)>(27, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 28 },
        }}),
        // Signal 'clipboardStateChanged'
        QtMocHelpers::SignalData<void()>(29, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'fileOperationStateChanged'
        QtMocHelpers::SignalData<void()>(30, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'pasteConflictStateChanged'
        QtMocHelpers::SignalData<void()>(31, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'deviceStateChanged'
        QtMocHelpers::SignalData<void()>(32, 2, QMC::AccessPublic, QMetaType::Void),
        // Slot 'handleModelChanged'
        QtMocHelpers::SlotData<void()>(33, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'handleListingOptionsChanged'
        QtMocHelpers::SlotData<void()>(34, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'persistCurrentPath'
        QtMocHelpers::SlotData<void()>(35, 2, QMC::AccessPrivate, QMetaType::Void),
        // Method 'navigateTo'
        QtMocHelpers::MethodData<BackendRequestId(const QString &)>(36, 2, QMC::AccessPublic, 0x80000000 | 37, {{
            { QMetaType::QString, 38 },
        }}),
        // Method 'submitSearch'
        QtMocHelpers::MethodData<BackendRequestId(const QString &, const QString &)>(39, 2, QMC::AccessPublic, 0x80000000 | 37, {{
            { QMetaType::QString, 40 }, { QMetaType::QString, 41 },
        }}),
        // Method 'submitSearch'
        QtMocHelpers::MethodData<BackendRequestId(const QString &)>(39, 2, QMC::AccessPublic | QMC::MethodCloned, 0x80000000 | 37, {{
            { QMetaType::QString, 40 },
        }}),
        // Method 'startSearch'
        QtMocHelpers::MethodData<void()>(42, 2, QMC::AccessPublic, QMetaType::Void),
        // Method 'hideSearch'
        QtMocHelpers::MethodData<void()>(43, 2, QMC::AccessPublic, QMetaType::Void),
        // Method 'clearSearch'
        QtMocHelpers::MethodData<void()>(44, 2, QMC::AccessPublic, QMetaType::Void),
        // Method 'goBack'
        QtMocHelpers::MethodData<void()>(45, 2, QMC::AccessPublic, QMetaType::Void),
        // Method 'goForward'
        QtMocHelpers::MethodData<void()>(46, 2, QMC::AccessPublic, QMetaType::Void),
        // Method 'createTab'
        QtMocHelpers::MethodData<void(const QString &)>(47, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 38 },
        }}),
        // Method 'createTab'
        QtMocHelpers::MethodData<void()>(47, 2, QMC::AccessPublic | QMC::MethodCloned, QMetaType::Void),
        // Method 'closeTab'
        QtMocHelpers::MethodData<void(int)>(48, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::Int, 49 },
        }}),
        // Method 'switchTab'
        QtMocHelpers::MethodData<void(int)>(50, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::Int, 49 },
        }}),
        // Method 'closeTabById'
        QtMocHelpers::MethodData<void(int)>(51, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::Int, 52 },
        }}),
        // Method 'switchTabById'
        QtMocHelpers::MethodData<void(int)>(53, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::Int, 52 },
        }}),
        // Method 'tabIndexById'
        QtMocHelpers::MethodData<int(int) const>(54, 2, QMC::AccessPublic, QMetaType::Int, {{
            { QMetaType::Int, 52 },
        }}),
        // Method 'moveTab'
        QtMocHelpers::MethodData<void(int, int)>(55, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::Int, 56 }, { QMetaType::Int, 57 },
        }}),
        // Method 'refreshCurrentFolder'
        QtMocHelpers::MethodData<BackendRequestId()>(58, 2, QMC::AccessPublic, 0x80000000 | 37),
        // Method 'replaceFileModel'
        QtMocHelpers::MethodData<bool(const QVariantList &)>(59, 2, QMC::AccessPublic, QMetaType::Bool, {{
            { 0x80000000 | 60, 61 },
        }}),
        // Method 'updateFileModelMetadata'
        QtMocHelpers::MethodData<int(const QVariantList &)>(62, 2, QMC::AccessPublic, QMetaType::Int, {{
            { 0x80000000 | 60, 61 },
        }}),
        // Method 'removePathsFromFileModel'
        QtMocHelpers::MethodData<int(const QStringList &)>(63, 2, QMC::AccessPublic, QMetaType::Int, {{
            { QMetaType::QStringList, 64 },
        }}),
        // Method 'increaseZoom'
        QtMocHelpers::MethodData<void()>(65, 2, QMC::AccessPublic, QMetaType::Void),
        // Method 'decreaseZoom'
        QtMocHelpers::MethodData<void()>(66, 2, QMC::AccessPublic, QMetaType::Void),
        // Method 'resetZoom'
        QtMocHelpers::MethodData<void()>(67, 2, QMC::AccessPublic, QMetaType::Void),
        // Method 'setZoom'
        QtMocHelpers::MethodData<void(double)>(68, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::Double, 69 },
        }}),
        // Method 'selectedItem'
        QtMocHelpers::MethodData<QVariant() const>(70, 2, QMC::AccessPublic, 0x80000000 | 71),
        // Method 'fileUrlForPath'
        QtMocHelpers::MethodData<QString(const QString &) const>(72, 2, QMC::AccessPublic, QMetaType::QString, {{
            { QMetaType::QString, 38 },
        }}),
        // Method 'joinPath'
        QtMocHelpers::MethodData<QString(const QString &, const QString &) const>(73, 2, QMC::AccessPublic, QMetaType::QString, {{
            { QMetaType::QString, 74 }, { QMetaType::QString, 75 },
        }}),
        // Method 'selectedPathsInCurrentFolder'
        QtMocHelpers::MethodData<QStringList() const>(76, 2, QMC::AccessPublic, QMetaType::QStringList),
        // Method 'selectedUriListInCurrentFolder'
        QtMocHelpers::MethodData<QString() const>(77, 2, QMC::AccessPublic, QMetaType::QString),
        // Method 'fileMatchesDialogFilter'
        QtMocHelpers::MethodData<bool(const QString &, bool) const>(78, 2, QMC::AccessPublic, QMetaType::Bool, {{
            { QMetaType::QString, 75 }, { QMetaType::Bool, 79 },
        }}),
        // Method 'dropFilePaths'
        QtMocHelpers::MethodData<void(const QStringList &, const QString &, const QString &)>(80, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QStringList, 64 }, { QMetaType::QString, 81 }, { QMetaType::QString, 82 },
        }}),
        // Method 'dropFilePaths'
        QtMocHelpers::MethodData<void(const QStringList &, const QString &)>(80, 2, QMC::AccessPublic | QMC::MethodCloned, QMetaType::Void, {{
            { QMetaType::QStringList, 64 }, { QMetaType::QString, 81 },
        }}),
        // Method 'dropFiles'
        QtMocHelpers::MethodData<void(const QVariantList &, const QString &, const QString &)>(83, 2, QMC::AccessPublic, QMetaType::Void, {{
            { 0x80000000 | 60, 84 }, { QMetaType::QString, 81 }, { QMetaType::QString, 82 },
        }}),
        // Method 'dropFiles'
        QtMocHelpers::MethodData<void(const QVariantList &, const QString &)>(83, 2, QMC::AccessPublic | QMC::MethodCloned, QMetaType::Void, {{
            { 0x80000000 | 60, 84 }, { QMetaType::QString, 81 },
        }}),
        // Method 'copySelected'
        QtMocHelpers::MethodData<void()>(85, 2, QMC::AccessPublic, QMetaType::Void),
        // Method 'cutSelected'
        QtMocHelpers::MethodData<void()>(86, 2, QMC::AccessPublic, QMetaType::Void),
        // Method 'pasteFiles'
        QtMocHelpers::MethodData<BackendRequestId()>(87, 2, QMC::AccessPublic, 0x80000000 | 37),
        // Method 'isCutPending'
        QtMocHelpers::MethodData<bool(const QString &) const>(88, 2, QMC::AccessPublic, QMetaType::Bool, {{
            { QMetaType::QString, 89 },
        }}),
        // Method 'resolvePasteConflict'
        QtMocHelpers::MethodData<void(const QString &)>(90, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 91 },
        }}),
        // Method 'renamePasteConflict'
        QtMocHelpers::MethodData<void(const QString &)>(92, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 89 },
        }}),
        // Method 'cancelPasteConflict'
        QtMocHelpers::MethodData<void()>(93, 2, QMC::AccessPublic, QMetaType::Void),
        // Method 'requestMountDevice'
        QtMocHelpers::MethodData<BackendRequestId(const QString &, bool, bool)>(94, 2, QMC::AccessPublic, 0x80000000 | 37, {{
            { QMetaType::QString, 95 }, { QMetaType::Bool, 96 }, { QMetaType::Bool, 97 },
        }}),
        // Method 'requestMountDevice'
        QtMocHelpers::MethodData<BackendRequestId(const QString &, bool)>(94, 2, QMC::AccessPublic | QMC::MethodCloned, 0x80000000 | 37, {{
            { QMetaType::QString, 95 }, { QMetaType::Bool, 96 },
        }}),
        // Method 'requestMountDevice'
        QtMocHelpers::MethodData<BackendRequestId(const QString &)>(94, 2, QMC::AccessPublic | QMC::MethodCloned, 0x80000000 | 37, {{
            { QMetaType::QString, 95 },
        }}),
        // Method 'requestUnmountDevice'
        QtMocHelpers::MethodData<BackendRequestId(const QString &, const QString &)>(98, 2, QMC::AccessPublic, 0x80000000 | 37, {{
            { QMetaType::QString, 95 }, { QMetaType::QString, 99 },
        }}),
        // Method 'requestRemountDevice'
        QtMocHelpers::MethodData<BackendRequestId(const QString &, const QString &, bool)>(100, 2, QMC::AccessPublic, 0x80000000 | 37, {{
            { QMetaType::QString, 95 }, { QMetaType::QString, 99 }, { QMetaType::Bool, 97 },
        }}),
        // Method 'requestRemountDevice'
        QtMocHelpers::MethodData<BackendRequestId(const QString &, const QString &)>(100, 2, QMC::AccessPublic | QMC::MethodCloned, 0x80000000 | 37, {{
            { QMetaType::QString, 95 }, { QMetaType::QString, 99 },
        }}),
        // Method 'toggleDeviceAutoMount'
        QtMocHelpers::MethodData<void(const QString &, bool)>(101, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 102 }, { QMetaType::Bool, 103 },
        }}),
        // Method 'isRecentPath'
        QtMocHelpers::MethodData<bool(const QString &) const>(104, 2, QMC::AccessPublic, QMetaType::Bool, {{
            { QMetaType::QString, 38 },
        }}),
        // Method 'isTrashPath'
        QtMocHelpers::MethodData<bool(const QString &) const>(105, 2, QMC::AccessPublic, QMetaType::Bool, {{
            { QMetaType::QString, 38 },
        }}),
        // Method 'canPinSidebarFavorite'
        QtMocHelpers::MethodData<bool(const QString &) const>(106, 2, QMC::AccessPublic, QMetaType::Bool, {{
            { QMetaType::QString, 38 },
        }}),
        // Method 'isSidebarFavorite'
        QtMocHelpers::MethodData<bool(const QString &) const>(107, 2, QMC::AccessPublic, QMetaType::Bool, {{
            { QMetaType::QString, 38 },
        }}),
        // Method 'visibleDefaultSidebarFavorites'
        QtMocHelpers::MethodData<QVariantList(const QVariantList &) const>(108, 2, QMC::AccessPublic, 0x80000000 | 60, {{
            { 0x80000000 | 60, 61 },
        }}),
        // Method 'pinSidebarFavorite'
        QtMocHelpers::MethodData<void(const QString &, const QString &, const QString &)>(109, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 38 }, { QMetaType::QString, 110 }, { QMetaType::QString, 111 },
        }}),
        // Method 'pinSidebarFavorite'
        QtMocHelpers::MethodData<void(const QString &, const QString &)>(109, 2, QMC::AccessPublic | QMC::MethodCloned, QMetaType::Void, {{
            { QMetaType::QString, 38 }, { QMetaType::QString, 110 },
        }}),
        // Method 'pinSidebarFavorite'
        QtMocHelpers::MethodData<void(const QString &)>(109, 2, QMC::AccessPublic | QMC::MethodCloned, QMetaType::Void, {{
            { QMetaType::QString, 38 },
        }}),
        // Method 'removeSidebarFavorite'
        QtMocHelpers::MethodData<void(const QString &)>(112, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 38 },
        }}),
        // Method 'announceContextMenuOpening'
        QtMocHelpers::MethodData<void(const QString &)>(113, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 28 },
        }}),
        // Method 'isSelected'
        QtMocHelpers::MethodData<bool(const QString &) const>(114, 2, QMC::AccessPublic, QMetaType::Bool, {{
            { QMetaType::QString, 89 },
        }}),
        // Method 'clearSelection'
        QtMocHelpers::MethodData<void()>(115, 2, QMC::AccessPublic, QMetaType::Void),
        // Method 'selectAll'
        QtMocHelpers::MethodData<void()>(116, 2, QMC::AccessPublic, QMetaType::Void),
        // Method 'selectByName'
        QtMocHelpers::MethodData<void(const QString &)>(117, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 89 },
        }}),
        // Method 'handleSelection'
        QtMocHelpers::MethodData<void(const QString &, int, bool, bool, bool)>(118, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 89 }, { QMetaType::Int, 49 }, { QMetaType::Bool, 119 }, { QMetaType::Bool, 120 },
            { QMetaType::Bool, 121 },
        }}),
    };
    QtMocHelpers::UintData qt_properties {
        // property 'fileModel'
        QtMocHelpers::PropertyData<QAbstractItemModel*>(122, 0x80000000 | 123, QMC::DefaultPropertyFlags | QMC::EnumOrFlag | QMC::Constant),
        // property 'homePath'
        QtMocHelpers::PropertyData<QString>(124, QMetaType::QString, QMC::DefaultPropertyFlags | QMC::Constant),
        // property 'runtimeRoot'
        QtMocHelpers::PropertyData<QString>(125, QMetaType::QString, QMC::DefaultPropertyFlags | QMC::Constant),
        // property 'backendPath'
        QtMocHelpers::PropertyData<QString>(126, QMetaType::QString, QMC::DefaultPropertyFlags | QMC::Constant),
        // property 'helperPath'
        QtMocHelpers::PropertyData<QString>(127, QMetaType::QString, QMC::DefaultPropertyFlags | QMC::Constant),
        // property 'wallpaperManagerPath'
        QtMocHelpers::PropertyData<QString>(128, QMetaType::QString, QMC::DefaultPropertyFlags | QMC::Constant),
        // property 'astreaLaunch'
        QtMocHelpers::PropertyData<QString>(129, QMetaType::QString, QMC::DefaultPropertyFlags | QMC::Constant),
        // property 'windowsRun'
        QtMocHelpers::PropertyData<QString>(130, QMetaType::QString, QMC::DefaultPropertyFlags | QMC::Constant),
        // property 'networkRootPath'
        QtMocHelpers::PropertyData<QString>(131, QMetaType::QString, QMC::DefaultPropertyFlags | QMC::Constant),
        // property 'trashFilesPath'
        QtMocHelpers::PropertyData<QString>(132, QMetaType::QString, QMC::DefaultPropertyFlags | QMC::Constant),
        // property 'trashInfoPath'
        QtMocHelpers::PropertyData<QString>(133, QMetaType::QString, QMC::DefaultPropertyFlags | QMC::Constant),
        // property 'recentVirtualPath'
        QtMocHelpers::PropertyData<QString>(134, QMetaType::QString, QMC::DefaultPropertyFlags | QMC::Constant),
        // property 'isPortalDialog'
        QtMocHelpers::PropertyData<bool>(135, QMetaType::Bool, QMC::DefaultPropertyFlags | QMC::Constant),
        // property 'currentPath'
        QtMocHelpers::PropertyData<QString>(136, QMetaType::QString, QMC::DefaultPropertyFlags, 0),
        // property 'history'
        QtMocHelpers::PropertyData<QStringList>(137, QMetaType::QStringList, QMC::DefaultPropertyFlags, 1),
        // property 'historyIdx'
        QtMocHelpers::PropertyData<int>(138, QMetaType::Int, QMC::DefaultPropertyFlags, 1),
        // property 'tabs'
        QtMocHelpers::PropertyData<QVariantList>(139, 0x80000000 | 60, QMC::DefaultPropertyFlags | QMC::EnumOrFlag, 2),
        // property 'breadcrumbParts'
        QtMocHelpers::PropertyData<QVariantList>(140, 0x80000000 | 60, QMC::DefaultPropertyFlags | QMC::EnumOrFlag, 0),
        // property 'activeTabIndex'
        QtMocHelpers::PropertyData<int>(141, QMetaType::Int, QMC::DefaultPropertyFlags, 3),
        // property 'loadingDir'
        QtMocHelpers::PropertyData<bool>(142, QMetaType::Bool, QMC::DefaultPropertyFlags, 4),
        // property 'loadError'
        QtMocHelpers::PropertyData<QString>(143, QMetaType::QString, QMC::DefaultPropertyFlags, 5),
        // property 'remoteDirectoryActive'
        QtMocHelpers::PropertyData<bool>(144, QMetaType::Bool, QMC::DefaultPropertyFlags, 6),
        // property 'searchActive'
        QtMocHelpers::PropertyData<bool>(145, QMetaType::Bool, QMC::DefaultPropertyFlags, 7),
        // property 'searchVisible'
        QtMocHelpers::PropertyData<bool>(146, QMetaType::Bool, QMC::DefaultPropertyFlags, 7),
        // property 'searchQuery'
        QtMocHelpers::PropertyData<QString>(147, QMetaType::QString, QMC::DefaultPropertyFlags | QMC::Writable | QMC::StdCppSet, 7),
        // property 'selectedFile'
        QtMocHelpers::PropertyData<QString>(148, QMetaType::QString, QMC::DefaultPropertyFlags | QMC::Writable | QMC::StdCppSet, 8),
        // property 'selectedFiles'
        QtMocHelpers::PropertyData<QStringList>(149, QMetaType::QStringList, QMC::DefaultPropertyFlags, 9),
        // property 'lastSelectedIndex'
        QtMocHelpers::PropertyData<int>(150, QMetaType::Int, QMC::DefaultPropertyFlags, 10),
        // property 'fileModelRevision'
        QtMocHelpers::PropertyData<int>(151, QMetaType::Int, QMC::DefaultPropertyFlags, 11),
        // property 'fileModelFilling'
        QtMocHelpers::PropertyData<bool>(152, QMetaType::Bool, QMC::DefaultPropertyFlags, 4),
        // property 'showPreview'
        QtMocHelpers::PropertyData<bool>(153, QMetaType::Bool, QMC::DefaultPropertyFlags | QMC::Writable | QMC::StdCppSet, 12),
        // property 'previewsEnabled'
        QtMocHelpers::PropertyData<bool>(154, QMetaType::Bool, QMC::DefaultPropertyFlags | QMC::Writable | QMC::StdCppSet, 12),
        // property 'viewMode'
        QtMocHelpers::PropertyData<QString>(155, QMetaType::QString, QMC::DefaultPropertyFlags | QMC::Writable | QMC::StdCppSet, 13),
        // property 'sortField'
        QtMocHelpers::PropertyData<QString>(156, QMetaType::QString, QMC::DefaultPropertyFlags | QMC::Writable | QMC::StdCppSet, 14),
        // property 'sortAsc'
        QtMocHelpers::PropertyData<bool>(157, QMetaType::Bool, QMC::DefaultPropertyFlags | QMC::Writable | QMC::StdCppSet, 15),
        // property 'showHidden'
        QtMocHelpers::PropertyData<bool>(158, QMetaType::Bool, QMC::DefaultPropertyFlags | QMC::Writable | QMC::StdCppSet, 16),
        // property 'foldersFirst'
        QtMocHelpers::PropertyData<bool>(159, QMetaType::Bool, QMC::DefaultPropertyFlags | QMC::Writable | QMC::StdCppSet, 17),
        // property 'groupingEnabled'
        QtMocHelpers::PropertyData<bool>(160, QMetaType::Bool, QMC::DefaultPropertyFlags | QMC::Writable | QMC::StdCppSet, 18),
        // property 'zoomLevel'
        QtMocHelpers::PropertyData<double>(161, QMetaType::Double, QMC::DefaultPropertyFlags | QMC::Writable | QMC::StdCppSet, 19),
        // property 'autoMountDeviceIdsJson'
        QtMocHelpers::PropertyData<QString>(162, QMetaType::QString, QMC::DefaultPropertyFlags | QMC::Writable | QMC::StdCppSet, 20),
        // property 'sidebarFavoritesJson'
        QtMocHelpers::PropertyData<QString>(163, QMetaType::QString, QMC::DefaultPropertyFlags | QMC::Writable | QMC::StdCppSet, 21),
        // property 'sidebarHiddenDefaultFavoritesJson'
        QtMocHelpers::PropertyData<QString>(164, QMetaType::QString, QMC::DefaultPropertyFlags | QMC::Writable | QMC::StdCppSet, 22),
        // property 'sidebarFavorites'
        QtMocHelpers::PropertyData<QVariantList>(165, 0x80000000 | 60, QMC::DefaultPropertyFlags | QMC::EnumOrFlag, 23),
        // property 'sidebarHiddenDefaultFavorites'
        QtMocHelpers::PropertyData<QVariantList>(166, 0x80000000 | 60, QMC::DefaultPropertyFlags | QMC::EnumOrFlag, 23),
        // property 'sidebarFavoritesRevision'
        QtMocHelpers::PropertyData<int>(167, QMetaType::Int, QMC::DefaultPropertyFlags, 23),
        // property 'defaultSidebarFavoritePaths'
        QtMocHelpers::PropertyData<QStringList>(168, QMetaType::QStringList, QMC::DefaultPropertyFlags | QMC::Constant),
        // property 'inTrashView'
        QtMocHelpers::PropertyData<bool>(169, QMetaType::Bool, QMC::DefaultPropertyFlags, 0),
        // property 'dialogActive'
        QtMocHelpers::PropertyData<bool>(170, QMetaType::Bool, QMC::DefaultPropertyFlags | QMC::Writable | QMC::StdCppSet, 24),
        // property 'dialogMode'
        QtMocHelpers::PropertyData<QString>(171, QMetaType::QString, QMC::DefaultPropertyFlags | QMC::Writable | QMC::StdCppSet, 24),
        // property 'dialogFilePatterns'
        QtMocHelpers::PropertyData<QStringList>(172, QMetaType::QStringList, QMC::DefaultPropertyFlags | QMC::Writable | QMC::StdCppSet, 24),
        // property 'clipboardFiles'
        QtMocHelpers::PropertyData<QStringList>(173, QMetaType::QStringList, QMC::DefaultPropertyFlags, 26),
        // property 'clipboardMode'
        QtMocHelpers::PropertyData<QString>(174, QMetaType::QString, QMC::DefaultPropertyFlags, 26),
        // property 'fileOperationRunning'
        QtMocHelpers::PropertyData<bool>(175, QMetaType::Bool, QMC::DefaultPropertyFlags, 27),
        // property 'fileOperationProgress'
        QtMocHelpers::PropertyData<double>(176, QMetaType::Double, QMC::DefaultPropertyFlags, 27),
        // property 'fileOperationPercent'
        QtMocHelpers::PropertyData<int>(177, QMetaType::Int, QMC::DefaultPropertyFlags, 27),
        // property 'fileOperationFileName'
        QtMocHelpers::PropertyData<QString>(178, QMetaType::QString, QMC::DefaultPropertyFlags, 27),
        // property 'fileOperationStatus'
        QtMocHelpers::PropertyData<QString>(179, QMetaType::QString, QMC::DefaultPropertyFlags, 27),
        // property 'fileOperationError'
        QtMocHelpers::PropertyData<QString>(180, QMetaType::QString, QMC::DefaultPropertyFlags, 27),
        // property 'fileOperationDestination'
        QtMocHelpers::PropertyData<QString>(181, QMetaType::QString, QMC::DefaultPropertyFlags, 27),
        // property 'fileOperationDoneCount'
        QtMocHelpers::PropertyData<int>(182, QMetaType::Int, QMC::DefaultPropertyFlags, 27),
        // property 'fileOperationTotalCount'
        QtMocHelpers::PropertyData<int>(183, QMetaType::Int, QMC::DefaultPropertyFlags, 27),
        // property 'fileOperationMode'
        QtMocHelpers::PropertyData<QString>(184, QMetaType::QString, QMC::DefaultPropertyFlags, 27),
        // property 'pasteConflictVisible'
        QtMocHelpers::PropertyData<bool>(185, QMetaType::Bool, QMC::DefaultPropertyFlags, 28),
        // property 'pasteConflictItems'
        QtMocHelpers::PropertyData<QVariantList>(186, 0x80000000 | 60, QMC::DefaultPropertyFlags | QMC::EnumOrFlag, 28),
        // property 'pendingPasteRename'
        QtMocHelpers::PropertyData<QString>(187, QMetaType::QString, QMC::DefaultPropertyFlags | QMC::Writable | QMC::StdCppSet, 28),
        // property 'deviceModel'
        QtMocHelpers::PropertyData<QVariantList>(188, 0x80000000 | 60, QMC::DefaultPropertyFlags | QMC::EnumOrFlag, 29),
        // property 'deviceError'
        QtMocHelpers::PropertyData<QString>(189, QMetaType::QString, QMC::DefaultPropertyFlags, 29),
    };
    QtMocHelpers::UintData qt_enums {
    };
    return QtMocHelpers::metaObjectData<AppStateFacade, qt_meta_tag_ZN6Astrea8Explorer6Native7Backend14AppStateFacadeE_t>(QMC::MetaObjectFlag{}, qt_stringData,
            qt_methods, qt_properties, qt_enums);
}
Q_CONSTINIT const QMetaObject Astrea::Explorer::Native::Backend::AppStateFacade::staticMetaObject = { {
    QMetaObject::SuperData::link<QObject::staticMetaObject>(),
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN6Astrea8Explorer6Native7Backend14AppStateFacadeE_t>.stringdata,
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN6Astrea8Explorer6Native7Backend14AppStateFacadeE_t>.data,
    qt_static_metacall,
    nullptr,
    qt_staticMetaObjectRelocatingContent<qt_meta_tag_ZN6Astrea8Explorer6Native7Backend14AppStateFacadeE_t>.metaTypes,
    nullptr
} };

void Astrea::Explorer::Native::Backend::AppStateFacade::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    auto *_t = static_cast<AppStateFacade *>(_o);
    if (_c == QMetaObject::InvokeMetaMethod) {
        switch (_id) {
        case 0: _t->currentPathChanged(); break;
        case 1: _t->historyChanged(); break;
        case 2: _t->tabsChanged(); break;
        case 3: _t->activeTabIndexChanged(); break;
        case 4: _t->loadingDirChanged(); break;
        case 5: _t->loadErrorChanged(); break;
        case 6: _t->remoteDirectoryActiveChanged(); break;
        case 7: _t->searchStateChanged(); break;
        case 8: _t->selectedFileChanged(); break;
        case 9: _t->selectedFilesChanged(); break;
        case 10: _t->lastSelectedIndexChanged(); break;
        case 11: _t->fileModelRevisionChanged(); break;
        case 12: _t->showPreviewChanged(); break;
        case 13: _t->viewModeChanged(); break;
        case 14: _t->sortFieldChanged(); break;
        case 15: _t->sortAscChanged(); break;
        case 16: _t->showHiddenChanged(); break;
        case 17: _t->foldersFirstChanged(); break;
        case 18: _t->groupingEnabledChanged(); break;
        case 19: _t->zoomLevelChanged(); break;
        case 20: _t->autoMountDeviceIdsJsonChanged(); break;
        case 21: _t->sidebarFavoritesJsonChanged(); break;
        case 22: _t->sidebarHiddenDefaultFavoritesJsonChanged(); break;
        case 23: _t->sidebarFavoritesChanged(); break;
        case 24: _t->dialogStateChanged(); break;
        case 25: _t->contextMenuOpening((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1]))); break;
        case 26: _t->clipboardStateChanged(); break;
        case 27: _t->fileOperationStateChanged(); break;
        case 28: _t->pasteConflictStateChanged(); break;
        case 29: _t->deviceStateChanged(); break;
        case 30: _t->handleModelChanged(); break;
        case 31: _t->handleListingOptionsChanged(); break;
        case 32: _t->persistCurrentPath(); break;
        case 33: { BackendRequestId _r = _t->navigateTo((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1])));
            if (_a[0]) *reinterpret_cast<BackendRequestId*>(_a[0]) = std::move(_r); }  break;
        case 34: { BackendRequestId _r = _t->submitSearch((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<QString>>(_a[2])));
            if (_a[0]) *reinterpret_cast<BackendRequestId*>(_a[0]) = std::move(_r); }  break;
        case 35: { BackendRequestId _r = _t->submitSearch((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1])));
            if (_a[0]) *reinterpret_cast<BackendRequestId*>(_a[0]) = std::move(_r); }  break;
        case 36: _t->startSearch(); break;
        case 37: _t->hideSearch(); break;
        case 38: _t->clearSearch(); break;
        case 39: _t->goBack(); break;
        case 40: _t->goForward(); break;
        case 41: _t->createTab((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1]))); break;
        case 42: _t->createTab(); break;
        case 43: _t->closeTab((*reinterpret_cast<std::add_pointer_t<int>>(_a[1]))); break;
        case 44: _t->switchTab((*reinterpret_cast<std::add_pointer_t<int>>(_a[1]))); break;
        case 45: _t->closeTabById((*reinterpret_cast<std::add_pointer_t<int>>(_a[1]))); break;
        case 46: _t->switchTabById((*reinterpret_cast<std::add_pointer_t<int>>(_a[1]))); break;
        case 47: { int _r = _t->tabIndexById((*reinterpret_cast<std::add_pointer_t<int>>(_a[1])));
            if (_a[0]) *reinterpret_cast<int*>(_a[0]) = std::move(_r); }  break;
        case 48: _t->moveTab((*reinterpret_cast<std::add_pointer_t<int>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<int>>(_a[2]))); break;
        case 49: { BackendRequestId _r = _t->refreshCurrentFolder();
            if (_a[0]) *reinterpret_cast<BackendRequestId*>(_a[0]) = std::move(_r); }  break;
        case 50: { bool _r = _t->replaceFileModel((*reinterpret_cast<std::add_pointer_t<QVariantList>>(_a[1])));
            if (_a[0]) *reinterpret_cast<bool*>(_a[0]) = std::move(_r); }  break;
        case 51: { int _r = _t->updateFileModelMetadata((*reinterpret_cast<std::add_pointer_t<QVariantList>>(_a[1])));
            if (_a[0]) *reinterpret_cast<int*>(_a[0]) = std::move(_r); }  break;
        case 52: { int _r = _t->removePathsFromFileModel((*reinterpret_cast<std::add_pointer_t<QStringList>>(_a[1])));
            if (_a[0]) *reinterpret_cast<int*>(_a[0]) = std::move(_r); }  break;
        case 53: _t->increaseZoom(); break;
        case 54: _t->decreaseZoom(); break;
        case 55: _t->resetZoom(); break;
        case 56: _t->setZoom((*reinterpret_cast<std::add_pointer_t<double>>(_a[1]))); break;
        case 57: { QVariant _r = _t->selectedItem();
            if (_a[0]) *reinterpret_cast<QVariant*>(_a[0]) = std::move(_r); }  break;
        case 58: { QString _r = _t->fileUrlForPath((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1])));
            if (_a[0]) *reinterpret_cast<QString*>(_a[0]) = std::move(_r); }  break;
        case 59: { QString _r = _t->joinPath((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<QString>>(_a[2])));
            if (_a[0]) *reinterpret_cast<QString*>(_a[0]) = std::move(_r); }  break;
        case 60: { QStringList _r = _t->selectedPathsInCurrentFolder();
            if (_a[0]) *reinterpret_cast<QStringList*>(_a[0]) = std::move(_r); }  break;
        case 61: { QString _r = _t->selectedUriListInCurrentFolder();
            if (_a[0]) *reinterpret_cast<QString*>(_a[0]) = std::move(_r); }  break;
        case 62: { bool _r = _t->fileMatchesDialogFilter((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<bool>>(_a[2])));
            if (_a[0]) *reinterpret_cast<bool*>(_a[0]) = std::move(_r); }  break;
        case 63: _t->dropFilePaths((*reinterpret_cast<std::add_pointer_t<QStringList>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<QString>>(_a[2])),(*reinterpret_cast<std::add_pointer_t<QString>>(_a[3]))); break;
        case 64: _t->dropFilePaths((*reinterpret_cast<std::add_pointer_t<QStringList>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<QString>>(_a[2]))); break;
        case 65: _t->dropFiles((*reinterpret_cast<std::add_pointer_t<QVariantList>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<QString>>(_a[2])),(*reinterpret_cast<std::add_pointer_t<QString>>(_a[3]))); break;
        case 66: _t->dropFiles((*reinterpret_cast<std::add_pointer_t<QVariantList>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<QString>>(_a[2]))); break;
        case 67: _t->copySelected(); break;
        case 68: _t->cutSelected(); break;
        case 69: { BackendRequestId _r = _t->pasteFiles();
            if (_a[0]) *reinterpret_cast<BackendRequestId*>(_a[0]) = std::move(_r); }  break;
        case 70: { bool _r = _t->isCutPending((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1])));
            if (_a[0]) *reinterpret_cast<bool*>(_a[0]) = std::move(_r); }  break;
        case 71: _t->resolvePasteConflict((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1]))); break;
        case 72: _t->renamePasteConflict((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1]))); break;
        case 73: _t->cancelPasteConflict(); break;
        case 74: { BackendRequestId _r = _t->requestMountDevice((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<bool>>(_a[2])),(*reinterpret_cast<std::add_pointer_t<bool>>(_a[3])));
            if (_a[0]) *reinterpret_cast<BackendRequestId*>(_a[0]) = std::move(_r); }  break;
        case 75: { BackendRequestId _r = _t->requestMountDevice((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<bool>>(_a[2])));
            if (_a[0]) *reinterpret_cast<BackendRequestId*>(_a[0]) = std::move(_r); }  break;
        case 76: { BackendRequestId _r = _t->requestMountDevice((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1])));
            if (_a[0]) *reinterpret_cast<BackendRequestId*>(_a[0]) = std::move(_r); }  break;
        case 77: { BackendRequestId _r = _t->requestUnmountDevice((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<QString>>(_a[2])));
            if (_a[0]) *reinterpret_cast<BackendRequestId*>(_a[0]) = std::move(_r); }  break;
        case 78: { BackendRequestId _r = _t->requestRemountDevice((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<QString>>(_a[2])),(*reinterpret_cast<std::add_pointer_t<bool>>(_a[3])));
            if (_a[0]) *reinterpret_cast<BackendRequestId*>(_a[0]) = std::move(_r); }  break;
        case 79: { BackendRequestId _r = _t->requestRemountDevice((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<QString>>(_a[2])));
            if (_a[0]) *reinterpret_cast<BackendRequestId*>(_a[0]) = std::move(_r); }  break;
        case 80: _t->toggleDeviceAutoMount((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<bool>>(_a[2]))); break;
        case 81: { bool _r = _t->isRecentPath((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1])));
            if (_a[0]) *reinterpret_cast<bool*>(_a[0]) = std::move(_r); }  break;
        case 82: { bool _r = _t->isTrashPath((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1])));
            if (_a[0]) *reinterpret_cast<bool*>(_a[0]) = std::move(_r); }  break;
        case 83: { bool _r = _t->canPinSidebarFavorite((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1])));
            if (_a[0]) *reinterpret_cast<bool*>(_a[0]) = std::move(_r); }  break;
        case 84: { bool _r = _t->isSidebarFavorite((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1])));
            if (_a[0]) *reinterpret_cast<bool*>(_a[0]) = std::move(_r); }  break;
        case 85: { QVariantList _r = _t->visibleDefaultSidebarFavorites((*reinterpret_cast<std::add_pointer_t<QVariantList>>(_a[1])));
            if (_a[0]) *reinterpret_cast<QVariantList*>(_a[0]) = std::move(_r); }  break;
        case 86: _t->pinSidebarFavorite((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<QString>>(_a[2])),(*reinterpret_cast<std::add_pointer_t<QString>>(_a[3]))); break;
        case 87: _t->pinSidebarFavorite((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<QString>>(_a[2]))); break;
        case 88: _t->pinSidebarFavorite((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1]))); break;
        case 89: _t->removeSidebarFavorite((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1]))); break;
        case 90: _t->announceContextMenuOpening((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1]))); break;
        case 91: { bool _r = _t->isSelected((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1])));
            if (_a[0]) *reinterpret_cast<bool*>(_a[0]) = std::move(_r); }  break;
        case 92: _t->clearSelection(); break;
        case 93: _t->selectAll(); break;
        case 94: _t->selectByName((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1]))); break;
        case 95: _t->handleSelection((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<int>>(_a[2])),(*reinterpret_cast<std::add_pointer_t<bool>>(_a[3])),(*reinterpret_cast<std::add_pointer_t<bool>>(_a[4])),(*reinterpret_cast<std::add_pointer_t<bool>>(_a[5]))); break;
        default: ;
        }
    }
    if (_c == QMetaObject::IndexOfMethod) {
        if (QtMocHelpers::indexOfMethod<void (AppStateFacade::*)()>(_a, &AppStateFacade::currentPathChanged, 0))
            return;
        if (QtMocHelpers::indexOfMethod<void (AppStateFacade::*)()>(_a, &AppStateFacade::historyChanged, 1))
            return;
        if (QtMocHelpers::indexOfMethod<void (AppStateFacade::*)()>(_a, &AppStateFacade::tabsChanged, 2))
            return;
        if (QtMocHelpers::indexOfMethod<void (AppStateFacade::*)()>(_a, &AppStateFacade::activeTabIndexChanged, 3))
            return;
        if (QtMocHelpers::indexOfMethod<void (AppStateFacade::*)()>(_a, &AppStateFacade::loadingDirChanged, 4))
            return;
        if (QtMocHelpers::indexOfMethod<void (AppStateFacade::*)()>(_a, &AppStateFacade::loadErrorChanged, 5))
            return;
        if (QtMocHelpers::indexOfMethod<void (AppStateFacade::*)()>(_a, &AppStateFacade::remoteDirectoryActiveChanged, 6))
            return;
        if (QtMocHelpers::indexOfMethod<void (AppStateFacade::*)()>(_a, &AppStateFacade::searchStateChanged, 7))
            return;
        if (QtMocHelpers::indexOfMethod<void (AppStateFacade::*)()>(_a, &AppStateFacade::selectedFileChanged, 8))
            return;
        if (QtMocHelpers::indexOfMethod<void (AppStateFacade::*)()>(_a, &AppStateFacade::selectedFilesChanged, 9))
            return;
        if (QtMocHelpers::indexOfMethod<void (AppStateFacade::*)()>(_a, &AppStateFacade::lastSelectedIndexChanged, 10))
            return;
        if (QtMocHelpers::indexOfMethod<void (AppStateFacade::*)()>(_a, &AppStateFacade::fileModelRevisionChanged, 11))
            return;
        if (QtMocHelpers::indexOfMethod<void (AppStateFacade::*)()>(_a, &AppStateFacade::showPreviewChanged, 12))
            return;
        if (QtMocHelpers::indexOfMethod<void (AppStateFacade::*)()>(_a, &AppStateFacade::viewModeChanged, 13))
            return;
        if (QtMocHelpers::indexOfMethod<void (AppStateFacade::*)()>(_a, &AppStateFacade::sortFieldChanged, 14))
            return;
        if (QtMocHelpers::indexOfMethod<void (AppStateFacade::*)()>(_a, &AppStateFacade::sortAscChanged, 15))
            return;
        if (QtMocHelpers::indexOfMethod<void (AppStateFacade::*)()>(_a, &AppStateFacade::showHiddenChanged, 16))
            return;
        if (QtMocHelpers::indexOfMethod<void (AppStateFacade::*)()>(_a, &AppStateFacade::foldersFirstChanged, 17))
            return;
        if (QtMocHelpers::indexOfMethod<void (AppStateFacade::*)()>(_a, &AppStateFacade::groupingEnabledChanged, 18))
            return;
        if (QtMocHelpers::indexOfMethod<void (AppStateFacade::*)()>(_a, &AppStateFacade::zoomLevelChanged, 19))
            return;
        if (QtMocHelpers::indexOfMethod<void (AppStateFacade::*)()>(_a, &AppStateFacade::autoMountDeviceIdsJsonChanged, 20))
            return;
        if (QtMocHelpers::indexOfMethod<void (AppStateFacade::*)()>(_a, &AppStateFacade::sidebarFavoritesJsonChanged, 21))
            return;
        if (QtMocHelpers::indexOfMethod<void (AppStateFacade::*)()>(_a, &AppStateFacade::sidebarHiddenDefaultFavoritesJsonChanged, 22))
            return;
        if (QtMocHelpers::indexOfMethod<void (AppStateFacade::*)()>(_a, &AppStateFacade::sidebarFavoritesChanged, 23))
            return;
        if (QtMocHelpers::indexOfMethod<void (AppStateFacade::*)()>(_a, &AppStateFacade::dialogStateChanged, 24))
            return;
        if (QtMocHelpers::indexOfMethod<void (AppStateFacade::*)(const QString & )>(_a, &AppStateFacade::contextMenuOpening, 25))
            return;
        if (QtMocHelpers::indexOfMethod<void (AppStateFacade::*)()>(_a, &AppStateFacade::clipboardStateChanged, 26))
            return;
        if (QtMocHelpers::indexOfMethod<void (AppStateFacade::*)()>(_a, &AppStateFacade::fileOperationStateChanged, 27))
            return;
        if (QtMocHelpers::indexOfMethod<void (AppStateFacade::*)()>(_a, &AppStateFacade::pasteConflictStateChanged, 28))
            return;
        if (QtMocHelpers::indexOfMethod<void (AppStateFacade::*)()>(_a, &AppStateFacade::deviceStateChanged, 29))
            return;
    }
    if (_c == QMetaObject::RegisterPropertyMetaType) {
        switch (_id) {
        default: *reinterpret_cast<int*>(_a[0]) = -1; break;
        case 0:
            *reinterpret_cast<int*>(_a[0]) = qRegisterMetaType< QAbstractItemModel* >(); break;
        }
    }
    if (_c == QMetaObject::ReadProperty) {
        void *_v = _a[0];
        switch (_id) {
        case 0: *reinterpret_cast<QAbstractItemModel**>(_v) = _t->fileModel(); break;
        case 1: *reinterpret_cast<QString*>(_v) = _t->homePath(); break;
        case 2: *reinterpret_cast<QString*>(_v) = _t->runtimeRoot(); break;
        case 3: *reinterpret_cast<QString*>(_v) = _t->backendPath(); break;
        case 4: *reinterpret_cast<QString*>(_v) = _t->helperPath(); break;
        case 5: *reinterpret_cast<QString*>(_v) = _t->wallpaperManagerPath(); break;
        case 6: *reinterpret_cast<QString*>(_v) = _t->astreaLaunch(); break;
        case 7: *reinterpret_cast<QString*>(_v) = _t->windowsRun(); break;
        case 8: *reinterpret_cast<QString*>(_v) = _t->networkRootPath(); break;
        case 9: *reinterpret_cast<QString*>(_v) = _t->trashFilesPath(); break;
        case 10: *reinterpret_cast<QString*>(_v) = _t->trashInfoPath(); break;
        case 11: *reinterpret_cast<QString*>(_v) = _t->recentVirtualPath(); break;
        case 12: *reinterpret_cast<bool*>(_v) = _t->isPortalDialog(); break;
        case 13: *reinterpret_cast<QString*>(_v) = _t->currentPath(); break;
        case 14: *reinterpret_cast<QStringList*>(_v) = _t->history(); break;
        case 15: *reinterpret_cast<int*>(_v) = _t->historyIdx(); break;
        case 16: *reinterpret_cast<QVariantList*>(_v) = _t->tabs(); break;
        case 17: *reinterpret_cast<QVariantList*>(_v) = _t->breadcrumbParts(); break;
        case 18: *reinterpret_cast<int*>(_v) = _t->activeTabIndex(); break;
        case 19: *reinterpret_cast<bool*>(_v) = _t->loadingDir(); break;
        case 20: *reinterpret_cast<QString*>(_v) = _t->loadError(); break;
        case 21: *reinterpret_cast<bool*>(_v) = _t->remoteDirectoryActive(); break;
        case 22: *reinterpret_cast<bool*>(_v) = _t->searchActive(); break;
        case 23: *reinterpret_cast<bool*>(_v) = _t->searchVisible(); break;
        case 24: *reinterpret_cast<QString*>(_v) = _t->searchQuery(); break;
        case 25: *reinterpret_cast<QString*>(_v) = _t->selectedFile(); break;
        case 26: *reinterpret_cast<QStringList*>(_v) = _t->selectedFiles(); break;
        case 27: *reinterpret_cast<int*>(_v) = _t->lastSelectedIndex(); break;
        case 28: *reinterpret_cast<int*>(_v) = _t->fileModelRevision(); break;
        case 29: *reinterpret_cast<bool*>(_v) = _t->fileModelFilling(); break;
        case 30: *reinterpret_cast<bool*>(_v) = _t->showPreview(); break;
        case 31: *reinterpret_cast<bool*>(_v) = _t->previewsEnabled(); break;
        case 32: *reinterpret_cast<QString*>(_v) = _t->viewMode(); break;
        case 33: *reinterpret_cast<QString*>(_v) = _t->sortField(); break;
        case 34: *reinterpret_cast<bool*>(_v) = _t->sortAsc(); break;
        case 35: *reinterpret_cast<bool*>(_v) = _t->showHidden(); break;
        case 36: *reinterpret_cast<bool*>(_v) = _t->foldersFirst(); break;
        case 37: *reinterpret_cast<bool*>(_v) = _t->groupingEnabled(); break;
        case 38: *reinterpret_cast<double*>(_v) = _t->zoomLevel(); break;
        case 39: *reinterpret_cast<QString*>(_v) = _t->autoMountDeviceIdsJson(); break;
        case 40: *reinterpret_cast<QString*>(_v) = _t->sidebarFavoritesJson(); break;
        case 41: *reinterpret_cast<QString*>(_v) = _t->sidebarHiddenDefaultFavoritesJson(); break;
        case 42: *reinterpret_cast<QVariantList*>(_v) = _t->sidebarFavorites(); break;
        case 43: *reinterpret_cast<QVariantList*>(_v) = _t->sidebarHiddenDefaultFavorites(); break;
        case 44: *reinterpret_cast<int*>(_v) = _t->sidebarFavoritesRevision(); break;
        case 45: *reinterpret_cast<QStringList*>(_v) = _t->defaultSidebarFavoritePaths(); break;
        case 46: *reinterpret_cast<bool*>(_v) = _t->inTrashView(); break;
        case 47: *reinterpret_cast<bool*>(_v) = _t->dialogActive(); break;
        case 48: *reinterpret_cast<QString*>(_v) = _t->dialogMode(); break;
        case 49: *reinterpret_cast<QStringList*>(_v) = _t->dialogFilePatterns(); break;
        case 50: *reinterpret_cast<QStringList*>(_v) = _t->clipboardFiles(); break;
        case 51: *reinterpret_cast<QString*>(_v) = _t->clipboardMode(); break;
        case 52: *reinterpret_cast<bool*>(_v) = _t->fileOperationRunning(); break;
        case 53: *reinterpret_cast<double*>(_v) = _t->fileOperationProgress(); break;
        case 54: *reinterpret_cast<int*>(_v) = _t->fileOperationPercent(); break;
        case 55: *reinterpret_cast<QString*>(_v) = _t->fileOperationFileName(); break;
        case 56: *reinterpret_cast<QString*>(_v) = _t->fileOperationStatus(); break;
        case 57: *reinterpret_cast<QString*>(_v) = _t->fileOperationError(); break;
        case 58: *reinterpret_cast<QString*>(_v) = _t->fileOperationDestination(); break;
        case 59: *reinterpret_cast<int*>(_v) = _t->fileOperationDoneCount(); break;
        case 60: *reinterpret_cast<int*>(_v) = _t->fileOperationTotalCount(); break;
        case 61: *reinterpret_cast<QString*>(_v) = _t->fileOperationMode(); break;
        case 62: *reinterpret_cast<bool*>(_v) = _t->pasteConflictVisible(); break;
        case 63: *reinterpret_cast<QVariantList*>(_v) = _t->pasteConflictItems(); break;
        case 64: *reinterpret_cast<QString*>(_v) = _t->pendingPasteRename(); break;
        case 65: *reinterpret_cast<QVariantList*>(_v) = _t->deviceModel(); break;
        case 66: *reinterpret_cast<QString*>(_v) = _t->deviceError(); break;
        default: break;
        }
    }
    if (_c == QMetaObject::WriteProperty) {
        void *_v = _a[0];
        switch (_id) {
        case 24: _t->setSearchQuery(*reinterpret_cast<QString*>(_v)); break;
        case 25: _t->setSelectedFile(*reinterpret_cast<QString*>(_v)); break;
        case 30: _t->setShowPreview(*reinterpret_cast<bool*>(_v)); break;
        case 31: _t->setPreviewsEnabled(*reinterpret_cast<bool*>(_v)); break;
        case 32: _t->setViewMode(*reinterpret_cast<QString*>(_v)); break;
        case 33: _t->setSortField(*reinterpret_cast<QString*>(_v)); break;
        case 34: _t->setSortAsc(*reinterpret_cast<bool*>(_v)); break;
        case 35: _t->setShowHidden(*reinterpret_cast<bool*>(_v)); break;
        case 36: _t->setFoldersFirst(*reinterpret_cast<bool*>(_v)); break;
        case 37: _t->setGroupingEnabled(*reinterpret_cast<bool*>(_v)); break;
        case 38: _t->setZoomLevel(*reinterpret_cast<double*>(_v)); break;
        case 39: _t->setAutoMountDeviceIdsJson(*reinterpret_cast<QString*>(_v)); break;
        case 40: _t->setSidebarFavoritesJson(*reinterpret_cast<QString*>(_v)); break;
        case 41: _t->setSidebarHiddenDefaultFavoritesJson(*reinterpret_cast<QString*>(_v)); break;
        case 47: _t->setDialogActive(*reinterpret_cast<bool*>(_v)); break;
        case 48: _t->setDialogMode(*reinterpret_cast<QString*>(_v)); break;
        case 49: _t->setDialogFilePatterns(*reinterpret_cast<QStringList*>(_v)); break;
        case 64: _t->setPendingPasteRename(*reinterpret_cast<QString*>(_v)); break;
        default: break;
        }
    }
}

const QMetaObject *Astrea::Explorer::Native::Backend::AppStateFacade::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *Astrea::Explorer::Native::Backend::AppStateFacade::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_staticMetaObjectStaticContent<qt_meta_tag_ZN6Astrea8Explorer6Native7Backend14AppStateFacadeE_t>.strings))
        return static_cast<void*>(this);
    return QObject::qt_metacast(_clname);
}

int Astrea::Explorer::Native::Backend::AppStateFacade::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QObject::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 96)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 96;
    }
    if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 96)
            *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType();
        _id -= 96;
    }
    if (_c == QMetaObject::ReadProperty || _c == QMetaObject::WriteProperty
            || _c == QMetaObject::ResetProperty || _c == QMetaObject::BindableProperty
            || _c == QMetaObject::RegisterPropertyMetaType) {
        qt_static_metacall(this, _c, _id, _a);
        _id -= 67;
    }
    return _id;
}

// SIGNAL 0
void Astrea::Explorer::Native::Backend::AppStateFacade::currentPathChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 0, nullptr);
}

// SIGNAL 1
void Astrea::Explorer::Native::Backend::AppStateFacade::historyChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 1, nullptr);
}

// SIGNAL 2
void Astrea::Explorer::Native::Backend::AppStateFacade::tabsChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 2, nullptr);
}

// SIGNAL 3
void Astrea::Explorer::Native::Backend::AppStateFacade::activeTabIndexChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 3, nullptr);
}

// SIGNAL 4
void Astrea::Explorer::Native::Backend::AppStateFacade::loadingDirChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 4, nullptr);
}

// SIGNAL 5
void Astrea::Explorer::Native::Backend::AppStateFacade::loadErrorChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 5, nullptr);
}

// SIGNAL 6
void Astrea::Explorer::Native::Backend::AppStateFacade::remoteDirectoryActiveChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 6, nullptr);
}

// SIGNAL 7
void Astrea::Explorer::Native::Backend::AppStateFacade::searchStateChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 7, nullptr);
}

// SIGNAL 8
void Astrea::Explorer::Native::Backend::AppStateFacade::selectedFileChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 8, nullptr);
}

// SIGNAL 9
void Astrea::Explorer::Native::Backend::AppStateFacade::selectedFilesChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 9, nullptr);
}

// SIGNAL 10
void Astrea::Explorer::Native::Backend::AppStateFacade::lastSelectedIndexChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 10, nullptr);
}

// SIGNAL 11
void Astrea::Explorer::Native::Backend::AppStateFacade::fileModelRevisionChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 11, nullptr);
}

// SIGNAL 12
void Astrea::Explorer::Native::Backend::AppStateFacade::showPreviewChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 12, nullptr);
}

// SIGNAL 13
void Astrea::Explorer::Native::Backend::AppStateFacade::viewModeChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 13, nullptr);
}

// SIGNAL 14
void Astrea::Explorer::Native::Backend::AppStateFacade::sortFieldChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 14, nullptr);
}

// SIGNAL 15
void Astrea::Explorer::Native::Backend::AppStateFacade::sortAscChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 15, nullptr);
}

// SIGNAL 16
void Astrea::Explorer::Native::Backend::AppStateFacade::showHiddenChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 16, nullptr);
}

// SIGNAL 17
void Astrea::Explorer::Native::Backend::AppStateFacade::foldersFirstChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 17, nullptr);
}

// SIGNAL 18
void Astrea::Explorer::Native::Backend::AppStateFacade::groupingEnabledChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 18, nullptr);
}

// SIGNAL 19
void Astrea::Explorer::Native::Backend::AppStateFacade::zoomLevelChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 19, nullptr);
}

// SIGNAL 20
void Astrea::Explorer::Native::Backend::AppStateFacade::autoMountDeviceIdsJsonChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 20, nullptr);
}

// SIGNAL 21
void Astrea::Explorer::Native::Backend::AppStateFacade::sidebarFavoritesJsonChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 21, nullptr);
}

// SIGNAL 22
void Astrea::Explorer::Native::Backend::AppStateFacade::sidebarHiddenDefaultFavoritesJsonChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 22, nullptr);
}

// SIGNAL 23
void Astrea::Explorer::Native::Backend::AppStateFacade::sidebarFavoritesChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 23, nullptr);
}

// SIGNAL 24
void Astrea::Explorer::Native::Backend::AppStateFacade::dialogStateChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 24, nullptr);
}

// SIGNAL 25
void Astrea::Explorer::Native::Backend::AppStateFacade::contextMenuOpening(const QString & _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 25, nullptr, _t1);
}

// SIGNAL 26
void Astrea::Explorer::Native::Backend::AppStateFacade::clipboardStateChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 26, nullptr);
}

// SIGNAL 27
void Astrea::Explorer::Native::Backend::AppStateFacade::fileOperationStateChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 27, nullptr);
}

// SIGNAL 28
void Astrea::Explorer::Native::Backend::AppStateFacade::pasteConflictStateChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 28, nullptr);
}

// SIGNAL 29
void Astrea::Explorer::Native::Backend::AppStateFacade::deviceStateChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 29, nullptr);
}
QT_WARNING_POP
