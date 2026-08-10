/****************************************************************************
** Meta object code from reading C++ file 'rust_backend_client.h'
**
** Created by: The Qt Meta Object Compiler version 69 (Qt 6.11.1)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../../../native/src/backend/rust_backend_client.h"
#include <QtCore/qmetatype.h>
#include <QtCore/QList>

#include <QtCore/qtmochelpers.h>

#include <memory>


#include <QtCore/qxptype_traits.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'rust_backend_client.h' doesn't include <QObject>."
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
struct qt_meta_tag_ZN6Astrea8Explorer6Native7Backend18IRustBackendClientE_t {};
} // unnamed namespace

template <> constexpr inline auto Astrea::Explorer::Native::Backend::IRustBackendClient::qt_create_metaobjectdata<qt_meta_tag_ZN6Astrea8Explorer6Native7Backend18IRustBackendClientE_t>()
{
    namespace QMC = QtMocConstants;
    QtMocHelpers::StringRefStorage qt_stringData {
        "Astrea::Explorer::Native::Backend::IRustBackendClient",
        "listReady",
        "",
        "Astrea::Explorer::Native::Backend::BackendRequestId",
        "requestId",
        "QList<Astrea::Explorer::Native::Backend::DirectoryEntry>",
        "entries",
        "searchReady",
        "devicesReady",
        "QList<Astrea::Explorer::Native::Backend::DeviceEntry>",
        "devices",
        "deviceOperationReady",
        "Astrea::Explorer::Native::Backend::DeviceOperationResult",
        "result",
        "fileOperationProgress",
        "Astrea::Explorer::Native::Backend::FileOperationProgress",
        "progress",
        "fileOperationReady",
        "Astrea::Explorer::Native::Backend::FileOperationResult",
        "failed",
        "Astrea::Explorer::Native::Backend::BackendError",
        "error",
        "cancel",
        "BackendRequestId"
    };

    QtMocHelpers::UintData qt_methods {
        // Signal 'listReady'
        QtMocHelpers::SignalData<void(Astrea::Explorer::Native::Backend::BackendRequestId, const QVector<Astrea::Explorer::Native::Backend::DirectoryEntry> &)>(1, 2, QMC::AccessPublic, QMetaType::Void, {{
            { 0x80000000 | 3, 4 }, { 0x80000000 | 5, 6 },
        }}),
        // Signal 'searchReady'
        QtMocHelpers::SignalData<void(Astrea::Explorer::Native::Backend::BackendRequestId, const QVector<Astrea::Explorer::Native::Backend::DirectoryEntry> &)>(7, 2, QMC::AccessPublic, QMetaType::Void, {{
            { 0x80000000 | 3, 4 }, { 0x80000000 | 5, 6 },
        }}),
        // Signal 'devicesReady'
        QtMocHelpers::SignalData<void(Astrea::Explorer::Native::Backend::BackendRequestId, const QVector<Astrea::Explorer::Native::Backend::DeviceEntry> &)>(8, 2, QMC::AccessPublic, QMetaType::Void, {{
            { 0x80000000 | 3, 4 }, { 0x80000000 | 9, 10 },
        }}),
        // Signal 'deviceOperationReady'
        QtMocHelpers::SignalData<void(Astrea::Explorer::Native::Backend::BackendRequestId, const Astrea::Explorer::Native::Backend::DeviceOperationResult &)>(11, 2, QMC::AccessPublic, QMetaType::Void, {{
            { 0x80000000 | 3, 4 }, { 0x80000000 | 12, 13 },
        }}),
        // Signal 'fileOperationProgress'
        QtMocHelpers::SignalData<void(Astrea::Explorer::Native::Backend::BackendRequestId, const Astrea::Explorer::Native::Backend::FileOperationProgress &)>(14, 2, QMC::AccessPublic, QMetaType::Void, {{
            { 0x80000000 | 3, 4 }, { 0x80000000 | 15, 16 },
        }}),
        // Signal 'fileOperationReady'
        QtMocHelpers::SignalData<void(Astrea::Explorer::Native::Backend::BackendRequestId, const Astrea::Explorer::Native::Backend::FileOperationResult &)>(17, 2, QMC::AccessPublic, QMetaType::Void, {{
            { 0x80000000 | 3, 4 }, { 0x80000000 | 18, 13 },
        }}),
        // Signal 'failed'
        QtMocHelpers::SignalData<void(const Astrea::Explorer::Native::Backend::BackendError &)>(19, 2, QMC::AccessPublic, QMetaType::Void, {{
            { 0x80000000 | 20, 21 },
        }}),
        // Slot 'cancel'
        QtMocHelpers::SlotData<void(BackendRequestId)>(22, 2, QMC::AccessPublic, QMetaType::Void, {{
            { 0x80000000 | 23, 4 },
        }}),
    };
    QtMocHelpers::UintData qt_properties {
    };
    QtMocHelpers::UintData qt_enums {
    };
    return QtMocHelpers::metaObjectData<IRustBackendClient, qt_meta_tag_ZN6Astrea8Explorer6Native7Backend18IRustBackendClientE_t>(QMC::MetaObjectFlag{}, qt_stringData,
            qt_methods, qt_properties, qt_enums);
}
Q_CONSTINIT const QMetaObject Astrea::Explorer::Native::Backend::IRustBackendClient::staticMetaObject = { {
    QMetaObject::SuperData::link<QObject::staticMetaObject>(),
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN6Astrea8Explorer6Native7Backend18IRustBackendClientE_t>.stringdata,
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN6Astrea8Explorer6Native7Backend18IRustBackendClientE_t>.data,
    qt_static_metacall,
    nullptr,
    qt_staticMetaObjectRelocatingContent<qt_meta_tag_ZN6Astrea8Explorer6Native7Backend18IRustBackendClientE_t>.metaTypes,
    nullptr
} };

void Astrea::Explorer::Native::Backend::IRustBackendClient::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    auto *_t = static_cast<IRustBackendClient *>(_o);
    if (_c == QMetaObject::InvokeMetaMethod) {
        switch (_id) {
        case 0: _t->listReady((*reinterpret_cast<std::add_pointer_t<Astrea::Explorer::Native::Backend::BackendRequestId>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<QList<Astrea::Explorer::Native::Backend::DirectoryEntry>>>(_a[2]))); break;
        case 1: _t->searchReady((*reinterpret_cast<std::add_pointer_t<Astrea::Explorer::Native::Backend::BackendRequestId>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<QList<Astrea::Explorer::Native::Backend::DirectoryEntry>>>(_a[2]))); break;
        case 2: _t->devicesReady((*reinterpret_cast<std::add_pointer_t<Astrea::Explorer::Native::Backend::BackendRequestId>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<QList<Astrea::Explorer::Native::Backend::DeviceEntry>>>(_a[2]))); break;
        case 3: _t->deviceOperationReady((*reinterpret_cast<std::add_pointer_t<Astrea::Explorer::Native::Backend::BackendRequestId>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<Astrea::Explorer::Native::Backend::DeviceOperationResult>>(_a[2]))); break;
        case 4: _t->fileOperationProgress((*reinterpret_cast<std::add_pointer_t<Astrea::Explorer::Native::Backend::BackendRequestId>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<Astrea::Explorer::Native::Backend::FileOperationProgress>>(_a[2]))); break;
        case 5: _t->fileOperationReady((*reinterpret_cast<std::add_pointer_t<Astrea::Explorer::Native::Backend::BackendRequestId>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<Astrea::Explorer::Native::Backend::FileOperationResult>>(_a[2]))); break;
        case 6: _t->failed((*reinterpret_cast<std::add_pointer_t<Astrea::Explorer::Native::Backend::BackendError>>(_a[1]))); break;
        case 7: _t->cancel((*reinterpret_cast<std::add_pointer_t<BackendRequestId>>(_a[1]))); break;
        default: ;
        }
    }
    if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        switch (_id) {
        default: *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType(); break;
        case 0:
            switch (*reinterpret_cast<int*>(_a[1])) {
            default: *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType(); break;
            case 1:
                *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType::fromType< QList<Astrea::Explorer::Native::Backend::DirectoryEntry> >(); break;
            }
            break;
        case 1:
            switch (*reinterpret_cast<int*>(_a[1])) {
            default: *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType(); break;
            case 1:
                *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType::fromType< QList<Astrea::Explorer::Native::Backend::DirectoryEntry> >(); break;
            }
            break;
        case 2:
            switch (*reinterpret_cast<int*>(_a[1])) {
            default: *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType(); break;
            case 1:
                *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType::fromType< QList<Astrea::Explorer::Native::Backend::DeviceEntry> >(); break;
            }
            break;
        case 3:
            switch (*reinterpret_cast<int*>(_a[1])) {
            default: *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType(); break;
            case 1:
                *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType::fromType< Astrea::Explorer::Native::Backend::DeviceOperationResult >(); break;
            }
            break;
        case 4:
            switch (*reinterpret_cast<int*>(_a[1])) {
            default: *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType(); break;
            case 1:
                *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType::fromType< Astrea::Explorer::Native::Backend::FileOperationProgress >(); break;
            }
            break;
        case 5:
            switch (*reinterpret_cast<int*>(_a[1])) {
            default: *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType(); break;
            case 1:
                *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType::fromType< Astrea::Explorer::Native::Backend::FileOperationResult >(); break;
            }
            break;
        case 6:
            switch (*reinterpret_cast<int*>(_a[1])) {
            default: *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType(); break;
            case 0:
                *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType::fromType< Astrea::Explorer::Native::Backend::BackendError >(); break;
            }
            break;
        }
    }
    if (_c == QMetaObject::IndexOfMethod) {
        if (QtMocHelpers::indexOfMethod<void (IRustBackendClient::*)(Astrea::Explorer::Native::Backend::BackendRequestId , const QVector<Astrea::Explorer::Native::Backend::DirectoryEntry> & )>(_a, &IRustBackendClient::listReady, 0))
            return;
        if (QtMocHelpers::indexOfMethod<void (IRustBackendClient::*)(Astrea::Explorer::Native::Backend::BackendRequestId , const QVector<Astrea::Explorer::Native::Backend::DirectoryEntry> & )>(_a, &IRustBackendClient::searchReady, 1))
            return;
        if (QtMocHelpers::indexOfMethod<void (IRustBackendClient::*)(Astrea::Explorer::Native::Backend::BackendRequestId , const QVector<Astrea::Explorer::Native::Backend::DeviceEntry> & )>(_a, &IRustBackendClient::devicesReady, 2))
            return;
        if (QtMocHelpers::indexOfMethod<void (IRustBackendClient::*)(Astrea::Explorer::Native::Backend::BackendRequestId , const Astrea::Explorer::Native::Backend::DeviceOperationResult & )>(_a, &IRustBackendClient::deviceOperationReady, 3))
            return;
        if (QtMocHelpers::indexOfMethod<void (IRustBackendClient::*)(Astrea::Explorer::Native::Backend::BackendRequestId , const Astrea::Explorer::Native::Backend::FileOperationProgress & )>(_a, &IRustBackendClient::fileOperationProgress, 4))
            return;
        if (QtMocHelpers::indexOfMethod<void (IRustBackendClient::*)(Astrea::Explorer::Native::Backend::BackendRequestId , const Astrea::Explorer::Native::Backend::FileOperationResult & )>(_a, &IRustBackendClient::fileOperationReady, 5))
            return;
        if (QtMocHelpers::indexOfMethod<void (IRustBackendClient::*)(const Astrea::Explorer::Native::Backend::BackendError & )>(_a, &IRustBackendClient::failed, 6))
            return;
    }
}

const QMetaObject *Astrea::Explorer::Native::Backend::IRustBackendClient::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *Astrea::Explorer::Native::Backend::IRustBackendClient::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_staticMetaObjectStaticContent<qt_meta_tag_ZN6Astrea8Explorer6Native7Backend18IRustBackendClientE_t>.strings))
        return static_cast<void*>(this);
    return QObject::qt_metacast(_clname);
}

int Astrea::Explorer::Native::Backend::IRustBackendClient::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QObject::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 8)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 8;
    }
    if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 8)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 8;
    }
    return _id;
}

// SIGNAL 0
void Astrea::Explorer::Native::Backend::IRustBackendClient::listReady(Astrea::Explorer::Native::Backend::BackendRequestId _t1, const QVector<Astrea::Explorer::Native::Backend::DirectoryEntry> & _t2)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 0, nullptr, _t1, _t2);
}

// SIGNAL 1
void Astrea::Explorer::Native::Backend::IRustBackendClient::searchReady(Astrea::Explorer::Native::Backend::BackendRequestId _t1, const QVector<Astrea::Explorer::Native::Backend::DirectoryEntry> & _t2)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 1, nullptr, _t1, _t2);
}

// SIGNAL 2
void Astrea::Explorer::Native::Backend::IRustBackendClient::devicesReady(Astrea::Explorer::Native::Backend::BackendRequestId _t1, const QVector<Astrea::Explorer::Native::Backend::DeviceEntry> & _t2)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 2, nullptr, _t1, _t2);
}

// SIGNAL 3
void Astrea::Explorer::Native::Backend::IRustBackendClient::deviceOperationReady(Astrea::Explorer::Native::Backend::BackendRequestId _t1, const Astrea::Explorer::Native::Backend::DeviceOperationResult & _t2)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 3, nullptr, _t1, _t2);
}

// SIGNAL 4
void Astrea::Explorer::Native::Backend::IRustBackendClient::fileOperationProgress(Astrea::Explorer::Native::Backend::BackendRequestId _t1, const Astrea::Explorer::Native::Backend::FileOperationProgress & _t2)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 4, nullptr, _t1, _t2);
}

// SIGNAL 5
void Astrea::Explorer::Native::Backend::IRustBackendClient::fileOperationReady(Astrea::Explorer::Native::Backend::BackendRequestId _t1, const Astrea::Explorer::Native::Backend::FileOperationResult & _t2)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 5, nullptr, _t1, _t2);
}

// SIGNAL 6
void Astrea::Explorer::Native::Backend::IRustBackendClient::failed(const Astrea::Explorer::Native::Backend::BackendError & _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 6, nullptr, _t1);
}
namespace {
struct qt_meta_tag_ZN6Astrea8Explorer6Native7Backend17RustBackendClientE_t {};
} // unnamed namespace

template <> constexpr inline auto Astrea::Explorer::Native::Backend::RustBackendClient::qt_create_metaobjectdata<qt_meta_tag_ZN6Astrea8Explorer6Native7Backend17RustBackendClientE_t>()
{
    namespace QMC = QtMocConstants;
    QtMocHelpers::StringRefStorage qt_stringData {
        "Astrea::Explorer::Native::Backend::RustBackendClient",
        "cancel",
        "",
        "BackendRequestId",
        "requestId"
    };

    QtMocHelpers::UintData qt_methods {
        // Slot 'cancel'
        QtMocHelpers::SlotData<void(BackendRequestId)>(1, 2, QMC::AccessPublic, QMetaType::Void, {{
            { 0x80000000 | 3, 4 },
        }}),
    };
    QtMocHelpers::UintData qt_properties {
    };
    QtMocHelpers::UintData qt_enums {
    };
    return QtMocHelpers::metaObjectData<RustBackendClient, qt_meta_tag_ZN6Astrea8Explorer6Native7Backend17RustBackendClientE_t>(QMC::MetaObjectFlag{}, qt_stringData,
            qt_methods, qt_properties, qt_enums);
}
Q_CONSTINIT const QMetaObject Astrea::Explorer::Native::Backend::RustBackendClient::staticMetaObject = { {
    QMetaObject::SuperData::link<IRustBackendClient::staticMetaObject>(),
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN6Astrea8Explorer6Native7Backend17RustBackendClientE_t>.stringdata,
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN6Astrea8Explorer6Native7Backend17RustBackendClientE_t>.data,
    qt_static_metacall,
    nullptr,
    qt_staticMetaObjectRelocatingContent<qt_meta_tag_ZN6Astrea8Explorer6Native7Backend17RustBackendClientE_t>.metaTypes,
    nullptr
} };

void Astrea::Explorer::Native::Backend::RustBackendClient::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    auto *_t = static_cast<RustBackendClient *>(_o);
    if (_c == QMetaObject::InvokeMetaMethod) {
        switch (_id) {
        case 0: _t->cancel((*reinterpret_cast<std::add_pointer_t<BackendRequestId>>(_a[1]))); break;
        default: ;
        }
    }
}

const QMetaObject *Astrea::Explorer::Native::Backend::RustBackendClient::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *Astrea::Explorer::Native::Backend::RustBackendClient::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_staticMetaObjectStaticContent<qt_meta_tag_ZN6Astrea8Explorer6Native7Backend17RustBackendClientE_t>.strings))
        return static_cast<void*>(this);
    return IRustBackendClient::qt_metacast(_clname);
}

int Astrea::Explorer::Native::Backend::RustBackendClient::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = IRustBackendClient::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 1)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 1;
    }
    if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 1)
            *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType();
        _id -= 1;
    }
    return _id;
}
QT_WARNING_POP
