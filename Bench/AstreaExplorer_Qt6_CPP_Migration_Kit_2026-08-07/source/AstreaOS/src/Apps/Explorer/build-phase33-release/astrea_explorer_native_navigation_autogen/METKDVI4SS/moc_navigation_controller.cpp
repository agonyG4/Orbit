/****************************************************************************
** Meta object code from reading C++ file 'navigation_controller.h'
**
** Created by: The Qt Meta Object Compiler version 69 (Qt 6.11.1)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../../../native/src/controllers/navigation_controller.h"
#include <QtCore/qmetatype.h>
#include <QtCore/QList>

#include <QtCore/qtmochelpers.h>

#include <memory>


#include <QtCore/qxptype_traits.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'navigation_controller.h' doesn't include <QObject>."
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
struct qt_meta_tag_ZN6Astrea8Explorer6Native7Backend20NavigationControllerE_t {};
} // unnamed namespace

template <> constexpr inline auto Astrea::Explorer::Native::Backend::NavigationController::qt_create_metaobjectdata<qt_meta_tag_ZN6Astrea8Explorer6Native7Backend20NavigationControllerE_t>()
{
    namespace QMC = QtMocConstants;
    QtMocHelpers::StringRefStorage qt_stringData {
        "Astrea::Explorer::Native::Backend::NavigationController",
        "currentPathChanged",
        "",
        "historyChanged",
        "tabsChanged",
        "activeTabIndexChanged",
        "loadingChanged",
        "loadErrorChanged",
        "searchStateChanged",
        "remoteStateChanged",
        "listingOptionsChanged",
        "navigationFailed",
        "Astrea::Explorer::Native::Backend::BackendError",
        "error",
        "handleListReady",
        "BackendRequestId",
        "requestId",
        "QList<DirectoryEntry>",
        "entries",
        "handleSearchReady",
        "handleBackendFailure",
        "BackendError",
        "handleDirectoryChanged",
        "path",
        "navigateTo",
        "submitSearch",
        "root",
        "query",
        "startSearch",
        "hideSearch",
        "clearSearch",
        "goBack",
        "goForward",
        "createTab",
        "initialPath",
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
        "currentPath",
        "history",
        "historyIndex",
        "tabs",
        "tabCount",
        "activeTabIndex",
        "loading",
        "loadError",
        "searchActive",
        "searchVisible",
        "searchQuery",
        "remoteDirectoryActive",
        "showHidden",
        "sortField",
        "sortAscending",
        "foldersFirst",
        "previews"
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
        // Signal 'loadingChanged'
        QtMocHelpers::SignalData<void()>(6, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'loadErrorChanged'
        QtMocHelpers::SignalData<void()>(7, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'searchStateChanged'
        QtMocHelpers::SignalData<void()>(8, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'remoteStateChanged'
        QtMocHelpers::SignalData<void()>(9, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'listingOptionsChanged'
        QtMocHelpers::SignalData<void()>(10, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'navigationFailed'
        QtMocHelpers::SignalData<void(const Astrea::Explorer::Native::Backend::BackendError &)>(11, 2, QMC::AccessPublic, QMetaType::Void, {{
            { 0x80000000 | 12, 13 },
        }}),
        // Slot 'handleListReady'
        QtMocHelpers::SlotData<void(BackendRequestId, const QVector<DirectoryEntry> &)>(14, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { 0x80000000 | 15, 16 }, { 0x80000000 | 17, 18 },
        }}),
        // Slot 'handleSearchReady'
        QtMocHelpers::SlotData<void(BackendRequestId, const QVector<DirectoryEntry> &)>(19, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { 0x80000000 | 15, 16 }, { 0x80000000 | 17, 18 },
        }}),
        // Slot 'handleBackendFailure'
        QtMocHelpers::SlotData<void(const BackendError &)>(20, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { 0x80000000 | 21, 13 },
        }}),
        // Slot 'handleDirectoryChanged'
        QtMocHelpers::SlotData<void(const QString &)>(22, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { QMetaType::QString, 23 },
        }}),
        // Method 'navigateTo'
        QtMocHelpers::MethodData<BackendRequestId(const QString &)>(24, 2, QMC::AccessPublic, 0x80000000 | 15, {{
            { QMetaType::QString, 23 },
        }}),
        // Method 'submitSearch'
        QtMocHelpers::MethodData<BackendRequestId(const QString &, const QString &)>(25, 2, QMC::AccessPublic, 0x80000000 | 15, {{
            { QMetaType::QString, 26 }, { QMetaType::QString, 27 },
        }}),
        // Method 'submitSearch'
        QtMocHelpers::MethodData<BackendRequestId(const QString &)>(25, 2, QMC::AccessPublic | QMC::MethodCloned, 0x80000000 | 15, {{
            { QMetaType::QString, 26 },
        }}),
        // Method 'startSearch'
        QtMocHelpers::MethodData<void()>(28, 2, QMC::AccessPublic, QMetaType::Void),
        // Method 'hideSearch'
        QtMocHelpers::MethodData<void()>(29, 2, QMC::AccessPublic, QMetaType::Void),
        // Method 'clearSearch'
        QtMocHelpers::MethodData<void()>(30, 2, QMC::AccessPublic, QMetaType::Void),
        // Method 'goBack'
        QtMocHelpers::MethodData<void()>(31, 2, QMC::AccessPublic, QMetaType::Void),
        // Method 'goForward'
        QtMocHelpers::MethodData<void()>(32, 2, QMC::AccessPublic, QMetaType::Void),
        // Method 'createTab'
        QtMocHelpers::MethodData<void(const QString &)>(33, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 34 },
        }}),
        // Method 'createTab'
        QtMocHelpers::MethodData<void()>(33, 2, QMC::AccessPublic | QMC::MethodCloned, QMetaType::Void),
        // Method 'closeTab'
        QtMocHelpers::MethodData<void(int)>(35, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::Int, 36 },
        }}),
        // Method 'switchTab'
        QtMocHelpers::MethodData<void(int)>(37, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::Int, 36 },
        }}),
        // Method 'closeTabById'
        QtMocHelpers::MethodData<void(int)>(38, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::Int, 39 },
        }}),
        // Method 'switchTabById'
        QtMocHelpers::MethodData<void(int)>(40, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::Int, 39 },
        }}),
        // Method 'tabIndexById'
        QtMocHelpers::MethodData<int(int) const>(41, 2, QMC::AccessPublic, QMetaType::Int, {{
            { QMetaType::Int, 39 },
        }}),
        // Method 'moveTab'
        QtMocHelpers::MethodData<void(int, int)>(42, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::Int, 43 }, { QMetaType::Int, 44 },
        }}),
        // Method 'refreshCurrentFolder'
        QtMocHelpers::MethodData<BackendRequestId()>(45, 2, QMC::AccessPublic, 0x80000000 | 15),
        // Method 'replaceFileModel'
        QtMocHelpers::MethodData<bool(const QVariantList &)>(46, 2, QMC::AccessPublic, QMetaType::Bool, {{
            { 0x80000000 | 47, 48 },
        }}),
        // Method 'updateFileModelMetadata'
        QtMocHelpers::MethodData<int(const QVariantList &)>(49, 2, QMC::AccessPublic, QMetaType::Int, {{
            { 0x80000000 | 47, 48 },
        }}),
        // Method 'removePathsFromFileModel'
        QtMocHelpers::MethodData<int(const QStringList &)>(50, 2, QMC::AccessPublic, QMetaType::Int, {{
            { QMetaType::QStringList, 51 },
        }}),
    };
    QtMocHelpers::UintData qt_properties {
        // property 'currentPath'
        QtMocHelpers::PropertyData<QString>(52, QMetaType::QString, QMC::DefaultPropertyFlags, 0),
        // property 'history'
        QtMocHelpers::PropertyData<QStringList>(53, QMetaType::QStringList, QMC::DefaultPropertyFlags, 1),
        // property 'historyIndex'
        QtMocHelpers::PropertyData<int>(54, QMetaType::Int, QMC::DefaultPropertyFlags, 1),
        // property 'tabs'
        QtMocHelpers::PropertyData<QVariantList>(55, 0x80000000 | 47, QMC::DefaultPropertyFlags | QMC::EnumOrFlag, 2),
        // property 'tabCount'
        QtMocHelpers::PropertyData<int>(56, QMetaType::Int, QMC::DefaultPropertyFlags, 2),
        // property 'activeTabIndex'
        QtMocHelpers::PropertyData<int>(57, QMetaType::Int, QMC::DefaultPropertyFlags, 3),
        // property 'loading'
        QtMocHelpers::PropertyData<bool>(58, QMetaType::Bool, QMC::DefaultPropertyFlags, 4),
        // property 'loadError'
        QtMocHelpers::PropertyData<QString>(59, QMetaType::QString, QMC::DefaultPropertyFlags, 5),
        // property 'searchActive'
        QtMocHelpers::PropertyData<bool>(60, QMetaType::Bool, QMC::DefaultPropertyFlags, 6),
        // property 'searchVisible'
        QtMocHelpers::PropertyData<bool>(61, QMetaType::Bool, QMC::DefaultPropertyFlags, 6),
        // property 'searchQuery'
        QtMocHelpers::PropertyData<QString>(62, QMetaType::QString, QMC::DefaultPropertyFlags, 6),
        // property 'remoteDirectoryActive'
        QtMocHelpers::PropertyData<bool>(63, QMetaType::Bool, QMC::DefaultPropertyFlags, 7),
        // property 'showHidden'
        QtMocHelpers::PropertyData<bool>(64, QMetaType::Bool, QMC::DefaultPropertyFlags, 8),
        // property 'sortField'
        QtMocHelpers::PropertyData<QString>(65, QMetaType::QString, QMC::DefaultPropertyFlags, 8),
        // property 'sortAscending'
        QtMocHelpers::PropertyData<bool>(66, QMetaType::Bool, QMC::DefaultPropertyFlags, 8),
        // property 'foldersFirst'
        QtMocHelpers::PropertyData<bool>(67, QMetaType::Bool, QMC::DefaultPropertyFlags, 8),
        // property 'previews'
        QtMocHelpers::PropertyData<bool>(68, QMetaType::Bool, QMC::DefaultPropertyFlags, 8),
    };
    QtMocHelpers::UintData qt_enums {
    };
    return QtMocHelpers::metaObjectData<NavigationController, qt_meta_tag_ZN6Astrea8Explorer6Native7Backend20NavigationControllerE_t>(QMC::MetaObjectFlag{}, qt_stringData,
            qt_methods, qt_properties, qt_enums);
}
Q_CONSTINIT const QMetaObject Astrea::Explorer::Native::Backend::NavigationController::staticMetaObject = { {
    QMetaObject::SuperData::link<QObject::staticMetaObject>(),
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN6Astrea8Explorer6Native7Backend20NavigationControllerE_t>.stringdata,
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN6Astrea8Explorer6Native7Backend20NavigationControllerE_t>.data,
    qt_static_metacall,
    nullptr,
    qt_staticMetaObjectRelocatingContent<qt_meta_tag_ZN6Astrea8Explorer6Native7Backend20NavigationControllerE_t>.metaTypes,
    nullptr
} };

void Astrea::Explorer::Native::Backend::NavigationController::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    auto *_t = static_cast<NavigationController *>(_o);
    if (_c == QMetaObject::InvokeMetaMethod) {
        switch (_id) {
        case 0: _t->currentPathChanged(); break;
        case 1: _t->historyChanged(); break;
        case 2: _t->tabsChanged(); break;
        case 3: _t->activeTabIndexChanged(); break;
        case 4: _t->loadingChanged(); break;
        case 5: _t->loadErrorChanged(); break;
        case 6: _t->searchStateChanged(); break;
        case 7: _t->remoteStateChanged(); break;
        case 8: _t->listingOptionsChanged(); break;
        case 9: _t->navigationFailed((*reinterpret_cast<std::add_pointer_t<Astrea::Explorer::Native::Backend::BackendError>>(_a[1]))); break;
        case 10: _t->handleListReady((*reinterpret_cast<std::add_pointer_t<BackendRequestId>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<QList<DirectoryEntry>>>(_a[2]))); break;
        case 11: _t->handleSearchReady((*reinterpret_cast<std::add_pointer_t<BackendRequestId>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<QList<DirectoryEntry>>>(_a[2]))); break;
        case 12: _t->handleBackendFailure((*reinterpret_cast<std::add_pointer_t<BackendError>>(_a[1]))); break;
        case 13: _t->handleDirectoryChanged((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1]))); break;
        case 14: { BackendRequestId _r = _t->navigateTo((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1])));
            if (_a[0]) *reinterpret_cast<BackendRequestId*>(_a[0]) = std::move(_r); }  break;
        case 15: { BackendRequestId _r = _t->submitSearch((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<QString>>(_a[2])));
            if (_a[0]) *reinterpret_cast<BackendRequestId*>(_a[0]) = std::move(_r); }  break;
        case 16: { BackendRequestId _r = _t->submitSearch((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1])));
            if (_a[0]) *reinterpret_cast<BackendRequestId*>(_a[0]) = std::move(_r); }  break;
        case 17: _t->startSearch(); break;
        case 18: _t->hideSearch(); break;
        case 19: _t->clearSearch(); break;
        case 20: _t->goBack(); break;
        case 21: _t->goForward(); break;
        case 22: _t->createTab((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1]))); break;
        case 23: _t->createTab(); break;
        case 24: _t->closeTab((*reinterpret_cast<std::add_pointer_t<int>>(_a[1]))); break;
        case 25: _t->switchTab((*reinterpret_cast<std::add_pointer_t<int>>(_a[1]))); break;
        case 26: _t->closeTabById((*reinterpret_cast<std::add_pointer_t<int>>(_a[1]))); break;
        case 27: _t->switchTabById((*reinterpret_cast<std::add_pointer_t<int>>(_a[1]))); break;
        case 28: { int _r = _t->tabIndexById((*reinterpret_cast<std::add_pointer_t<int>>(_a[1])));
            if (_a[0]) *reinterpret_cast<int*>(_a[0]) = std::move(_r); }  break;
        case 29: _t->moveTab((*reinterpret_cast<std::add_pointer_t<int>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<int>>(_a[2]))); break;
        case 30: { BackendRequestId _r = _t->refreshCurrentFolder();
            if (_a[0]) *reinterpret_cast<BackendRequestId*>(_a[0]) = std::move(_r); }  break;
        case 31: { bool _r = _t->replaceFileModel((*reinterpret_cast<std::add_pointer_t<QVariantList>>(_a[1])));
            if (_a[0]) *reinterpret_cast<bool*>(_a[0]) = std::move(_r); }  break;
        case 32: { int _r = _t->updateFileModelMetadata((*reinterpret_cast<std::add_pointer_t<QVariantList>>(_a[1])));
            if (_a[0]) *reinterpret_cast<int*>(_a[0]) = std::move(_r); }  break;
        case 33: { int _r = _t->removePathsFromFileModel((*reinterpret_cast<std::add_pointer_t<QStringList>>(_a[1])));
            if (_a[0]) *reinterpret_cast<int*>(_a[0]) = std::move(_r); }  break;
        default: ;
        }
    }
    if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        switch (_id) {
        default: *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType(); break;
        case 9:
            switch (*reinterpret_cast<int*>(_a[1])) {
            default: *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType(); break;
            case 0:
                *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType::fromType< Astrea::Explorer::Native::Backend::BackendError >(); break;
            }
            break;
        }
    }
    if (_c == QMetaObject::IndexOfMethod) {
        if (QtMocHelpers::indexOfMethod<void (NavigationController::*)()>(_a, &NavigationController::currentPathChanged, 0))
            return;
        if (QtMocHelpers::indexOfMethod<void (NavigationController::*)()>(_a, &NavigationController::historyChanged, 1))
            return;
        if (QtMocHelpers::indexOfMethod<void (NavigationController::*)()>(_a, &NavigationController::tabsChanged, 2))
            return;
        if (QtMocHelpers::indexOfMethod<void (NavigationController::*)()>(_a, &NavigationController::activeTabIndexChanged, 3))
            return;
        if (QtMocHelpers::indexOfMethod<void (NavigationController::*)()>(_a, &NavigationController::loadingChanged, 4))
            return;
        if (QtMocHelpers::indexOfMethod<void (NavigationController::*)()>(_a, &NavigationController::loadErrorChanged, 5))
            return;
        if (QtMocHelpers::indexOfMethod<void (NavigationController::*)()>(_a, &NavigationController::searchStateChanged, 6))
            return;
        if (QtMocHelpers::indexOfMethod<void (NavigationController::*)()>(_a, &NavigationController::remoteStateChanged, 7))
            return;
        if (QtMocHelpers::indexOfMethod<void (NavigationController::*)()>(_a, &NavigationController::listingOptionsChanged, 8))
            return;
        if (QtMocHelpers::indexOfMethod<void (NavigationController::*)(const Astrea::Explorer::Native::Backend::BackendError & )>(_a, &NavigationController::navigationFailed, 9))
            return;
    }
    if (_c == QMetaObject::ReadProperty) {
        void *_v = _a[0];
        switch (_id) {
        case 0: *reinterpret_cast<QString*>(_v) = _t->currentPath(); break;
        case 1: *reinterpret_cast<QStringList*>(_v) = _t->history(); break;
        case 2: *reinterpret_cast<int*>(_v) = _t->historyIndex(); break;
        case 3: *reinterpret_cast<QVariantList*>(_v) = _t->tabs(); break;
        case 4: *reinterpret_cast<int*>(_v) = _t->tabCount(); break;
        case 5: *reinterpret_cast<int*>(_v) = _t->activeTabIndex(); break;
        case 6: *reinterpret_cast<bool*>(_v) = _t->loading(); break;
        case 7: *reinterpret_cast<QString*>(_v) = _t->loadError(); break;
        case 8: *reinterpret_cast<bool*>(_v) = _t->searchActive(); break;
        case 9: *reinterpret_cast<bool*>(_v) = _t->searchVisible(); break;
        case 10: *reinterpret_cast<QString*>(_v) = _t->searchQuery(); break;
        case 11: *reinterpret_cast<bool*>(_v) = _t->remoteDirectoryActive(); break;
        case 12: *reinterpret_cast<bool*>(_v) = _t->showHidden(); break;
        case 13: *reinterpret_cast<QString*>(_v) = _t->sortField(); break;
        case 14: *reinterpret_cast<bool*>(_v) = _t->sortAscending(); break;
        case 15: *reinterpret_cast<bool*>(_v) = _t->foldersFirst(); break;
        case 16: *reinterpret_cast<bool*>(_v) = _t->previews(); break;
        default: break;
        }
    }
}

const QMetaObject *Astrea::Explorer::Native::Backend::NavigationController::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *Astrea::Explorer::Native::Backend::NavigationController::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_staticMetaObjectStaticContent<qt_meta_tag_ZN6Astrea8Explorer6Native7Backend20NavigationControllerE_t>.strings))
        return static_cast<void*>(this);
    return QObject::qt_metacast(_clname);
}

int Astrea::Explorer::Native::Backend::NavigationController::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QObject::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 34)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 34;
    }
    if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 34)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 34;
    }
    if (_c == QMetaObject::ReadProperty || _c == QMetaObject::WriteProperty
            || _c == QMetaObject::ResetProperty || _c == QMetaObject::BindableProperty
            || _c == QMetaObject::RegisterPropertyMetaType) {
        qt_static_metacall(this, _c, _id, _a);
        _id -= 17;
    }
    return _id;
}

// SIGNAL 0
void Astrea::Explorer::Native::Backend::NavigationController::currentPathChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 0, nullptr);
}

// SIGNAL 1
void Astrea::Explorer::Native::Backend::NavigationController::historyChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 1, nullptr);
}

// SIGNAL 2
void Astrea::Explorer::Native::Backend::NavigationController::tabsChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 2, nullptr);
}

// SIGNAL 3
void Astrea::Explorer::Native::Backend::NavigationController::activeTabIndexChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 3, nullptr);
}

// SIGNAL 4
void Astrea::Explorer::Native::Backend::NavigationController::loadingChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 4, nullptr);
}

// SIGNAL 5
void Astrea::Explorer::Native::Backend::NavigationController::loadErrorChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 5, nullptr);
}

// SIGNAL 6
void Astrea::Explorer::Native::Backend::NavigationController::searchStateChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 6, nullptr);
}

// SIGNAL 7
void Astrea::Explorer::Native::Backend::NavigationController::remoteStateChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 7, nullptr);
}

// SIGNAL 8
void Astrea::Explorer::Native::Backend::NavigationController::listingOptionsChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 8, nullptr);
}

// SIGNAL 9
void Astrea::Explorer::Native::Backend::NavigationController::navigationFailed(const Astrea::Explorer::Native::Backend::BackendError & _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 9, nullptr, _t1);
}
QT_WARNING_POP
