/****************************************************************************
** Meta object code from reading C++ file 'device_controller.h'
**
** Created by: The Qt Meta Object Compiler version 69 (Qt 6.11.1)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../../../native/src/controllers/device_controller.h"
#include <QtCore/qmetatype.h>
#include <QtCore/QList>

#include <QtCore/qtmochelpers.h>

#include <memory>


#include <QtCore/qxptype_traits.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'device_controller.h' doesn't include <QObject>."
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
struct qt_meta_tag_ZN6Astrea8Explorer6Native7Backend16DeviceControllerE_t {};
} // unnamed namespace

template <> constexpr inline auto Astrea::Explorer::Native::Backend::DeviceController::qt_create_metaobjectdata<qt_meta_tag_ZN6Astrea8Explorer6Native7Backend16DeviceControllerE_t>()
{
    namespace QMC = QtMocConstants;
    QtMocHelpers::StringRefStorage qt_stringData {
        "Astrea::Explorer::Native::Backend::DeviceController",
        "devicesChanged",
        "",
        "loadingChanged",
        "errorChanged",
        "operationChanged",
        "autoMountChanged",
        "operationFinished",
        "Astrea::Explorer::Native::Backend::DeviceOperationResult",
        "result",
        "setAutoMount",
        "deviceId",
        "enabled",
        "handleDevicesReady",
        "BackendRequestId",
        "requestId",
        "QList<DeviceEntry>",
        "devices",
        "handleDeviceOperationReady",
        "DeviceOperationResult",
        "handleBackendFailure",
        "BackendError",
        "error",
        "loading",
        "operationPath",
        "operationType",
        "operationTargetMountPath",
        "operationOpenAfterMount",
        "lastMountPath",
        "lastUnmountedMountPath",
        "operationError",
        "autoMountDeviceIdsJson"
    };

    QtMocHelpers::UintData qt_methods {
        // Signal 'devicesChanged'
        QtMocHelpers::SignalData<void()>(1, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'loadingChanged'
        QtMocHelpers::SignalData<void()>(3, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'errorChanged'
        QtMocHelpers::SignalData<void()>(4, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'operationChanged'
        QtMocHelpers::SignalData<void()>(5, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'autoMountChanged'
        QtMocHelpers::SignalData<void()>(6, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'operationFinished'
        QtMocHelpers::SignalData<void(const Astrea::Explorer::Native::Backend::DeviceOperationResult &)>(7, 2, QMC::AccessPublic, QMetaType::Void, {{
            { 0x80000000 | 8, 9 },
        }}),
        // Slot 'setAutoMount'
        QtMocHelpers::SlotData<void(const QString &, bool)>(10, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 11 }, { QMetaType::Bool, 12 },
        }}),
        // Slot 'handleDevicesReady'
        QtMocHelpers::SlotData<void(BackendRequestId, const QVector<DeviceEntry> &)>(13, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { 0x80000000 | 14, 15 }, { 0x80000000 | 16, 17 },
        }}),
        // Slot 'handleDeviceOperationReady'
        QtMocHelpers::SlotData<void(BackendRequestId, const DeviceOperationResult &)>(18, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { 0x80000000 | 14, 15 }, { 0x80000000 | 19, 9 },
        }}),
        // Slot 'handleBackendFailure'
        QtMocHelpers::SlotData<void(const BackendError &)>(20, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { 0x80000000 | 21, 22 },
        }}),
    };
    QtMocHelpers::UintData qt_properties {
        // property 'loading'
        QtMocHelpers::PropertyData<bool>(23, QMetaType::Bool, QMC::DefaultPropertyFlags, 1),
        // property 'error'
        QtMocHelpers::PropertyData<QString>(22, QMetaType::QString, QMC::DefaultPropertyFlags, 2),
        // property 'operationPath'
        QtMocHelpers::PropertyData<QString>(24, QMetaType::QString, QMC::DefaultPropertyFlags, 3),
        // property 'operationType'
        QtMocHelpers::PropertyData<QString>(25, QMetaType::QString, QMC::DefaultPropertyFlags, 3),
        // property 'operationTargetMountPath'
        QtMocHelpers::PropertyData<QString>(26, QMetaType::QString, QMC::DefaultPropertyFlags, 3),
        // property 'operationOpenAfterMount'
        QtMocHelpers::PropertyData<bool>(27, QMetaType::Bool, QMC::DefaultPropertyFlags, 3),
        // property 'lastMountPath'
        QtMocHelpers::PropertyData<QString>(28, QMetaType::QString, QMC::DefaultPropertyFlags, 3),
        // property 'lastUnmountedMountPath'
        QtMocHelpers::PropertyData<QString>(29, QMetaType::QString, QMC::DefaultPropertyFlags, 3),
        // property 'operationError'
        QtMocHelpers::PropertyData<QString>(30, QMetaType::QString, QMC::DefaultPropertyFlags, 3),
        // property 'autoMountDeviceIdsJson'
        QtMocHelpers::PropertyData<QString>(31, QMetaType::QString, QMC::DefaultPropertyFlags, 4),
    };
    QtMocHelpers::UintData qt_enums {
    };
    return QtMocHelpers::metaObjectData<DeviceController, qt_meta_tag_ZN6Astrea8Explorer6Native7Backend16DeviceControllerE_t>(QMC::MetaObjectFlag{}, qt_stringData,
            qt_methods, qt_properties, qt_enums);
}
Q_CONSTINIT const QMetaObject Astrea::Explorer::Native::Backend::DeviceController::staticMetaObject = { {
    QMetaObject::SuperData::link<QObject::staticMetaObject>(),
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN6Astrea8Explorer6Native7Backend16DeviceControllerE_t>.stringdata,
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN6Astrea8Explorer6Native7Backend16DeviceControllerE_t>.data,
    qt_static_metacall,
    nullptr,
    qt_staticMetaObjectRelocatingContent<qt_meta_tag_ZN6Astrea8Explorer6Native7Backend16DeviceControllerE_t>.metaTypes,
    nullptr
} };

void Astrea::Explorer::Native::Backend::DeviceController::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    auto *_t = static_cast<DeviceController *>(_o);
    if (_c == QMetaObject::InvokeMetaMethod) {
        switch (_id) {
        case 0: _t->devicesChanged(); break;
        case 1: _t->loadingChanged(); break;
        case 2: _t->errorChanged(); break;
        case 3: _t->operationChanged(); break;
        case 4: _t->autoMountChanged(); break;
        case 5: _t->operationFinished((*reinterpret_cast<std::add_pointer_t<Astrea::Explorer::Native::Backend::DeviceOperationResult>>(_a[1]))); break;
        case 6: _t->setAutoMount((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<bool>>(_a[2]))); break;
        case 7: _t->handleDevicesReady((*reinterpret_cast<std::add_pointer_t<BackendRequestId>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<QList<DeviceEntry>>>(_a[2]))); break;
        case 8: _t->handleDeviceOperationReady((*reinterpret_cast<std::add_pointer_t<BackendRequestId>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<DeviceOperationResult>>(_a[2]))); break;
        case 9: _t->handleBackendFailure((*reinterpret_cast<std::add_pointer_t<BackendError>>(_a[1]))); break;
        default: ;
        }
    }
    if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        switch (_id) {
        default: *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType(); break;
        case 5:
            switch (*reinterpret_cast<int*>(_a[1])) {
            default: *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType(); break;
            case 0:
                *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType::fromType< Astrea::Explorer::Native::Backend::DeviceOperationResult >(); break;
            }
            break;
        }
    }
    if (_c == QMetaObject::IndexOfMethod) {
        if (QtMocHelpers::indexOfMethod<void (DeviceController::*)()>(_a, &DeviceController::devicesChanged, 0))
            return;
        if (QtMocHelpers::indexOfMethod<void (DeviceController::*)()>(_a, &DeviceController::loadingChanged, 1))
            return;
        if (QtMocHelpers::indexOfMethod<void (DeviceController::*)()>(_a, &DeviceController::errorChanged, 2))
            return;
        if (QtMocHelpers::indexOfMethod<void (DeviceController::*)()>(_a, &DeviceController::operationChanged, 3))
            return;
        if (QtMocHelpers::indexOfMethod<void (DeviceController::*)()>(_a, &DeviceController::autoMountChanged, 4))
            return;
        if (QtMocHelpers::indexOfMethod<void (DeviceController::*)(const Astrea::Explorer::Native::Backend::DeviceOperationResult & )>(_a, &DeviceController::operationFinished, 5))
            return;
    }
    if (_c == QMetaObject::ReadProperty) {
        void *_v = _a[0];
        switch (_id) {
        case 0: *reinterpret_cast<bool*>(_v) = _t->loading(); break;
        case 1: *reinterpret_cast<QString*>(_v) = _t->error(); break;
        case 2: *reinterpret_cast<QString*>(_v) = _t->operationPath(); break;
        case 3: *reinterpret_cast<QString*>(_v) = _t->operationType(); break;
        case 4: *reinterpret_cast<QString*>(_v) = _t->operationTargetMountPath(); break;
        case 5: *reinterpret_cast<bool*>(_v) = _t->operationOpenAfterMount(); break;
        case 6: *reinterpret_cast<QString*>(_v) = _t->lastMountPath(); break;
        case 7: *reinterpret_cast<QString*>(_v) = _t->lastUnmountedMountPath(); break;
        case 8: *reinterpret_cast<QString*>(_v) = _t->operationError(); break;
        case 9: *reinterpret_cast<QString*>(_v) = _t->autoMountDeviceIdsJson(); break;
        default: break;
        }
    }
}

const QMetaObject *Astrea::Explorer::Native::Backend::DeviceController::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *Astrea::Explorer::Native::Backend::DeviceController::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_staticMetaObjectStaticContent<qt_meta_tag_ZN6Astrea8Explorer6Native7Backend16DeviceControllerE_t>.strings))
        return static_cast<void*>(this);
    return QObject::qt_metacast(_clname);
}

int Astrea::Explorer::Native::Backend::DeviceController::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QObject::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 10)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 10;
    }
    if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 10)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 10;
    }
    if (_c == QMetaObject::ReadProperty || _c == QMetaObject::WriteProperty
            || _c == QMetaObject::ResetProperty || _c == QMetaObject::BindableProperty
            || _c == QMetaObject::RegisterPropertyMetaType) {
        qt_static_metacall(this, _c, _id, _a);
        _id -= 10;
    }
    return _id;
}

// SIGNAL 0
void Astrea::Explorer::Native::Backend::DeviceController::devicesChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 0, nullptr);
}

// SIGNAL 1
void Astrea::Explorer::Native::Backend::DeviceController::loadingChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 1, nullptr);
}

// SIGNAL 2
void Astrea::Explorer::Native::Backend::DeviceController::errorChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 2, nullptr);
}

// SIGNAL 3
void Astrea::Explorer::Native::Backend::DeviceController::operationChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 3, nullptr);
}

// SIGNAL 4
void Astrea::Explorer::Native::Backend::DeviceController::autoMountChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 4, nullptr);
}

// SIGNAL 5
void Astrea::Explorer::Native::Backend::DeviceController::operationFinished(const Astrea::Explorer::Native::Backend::DeviceOperationResult & _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 5, nullptr, _t1);
}
QT_WARNING_POP
