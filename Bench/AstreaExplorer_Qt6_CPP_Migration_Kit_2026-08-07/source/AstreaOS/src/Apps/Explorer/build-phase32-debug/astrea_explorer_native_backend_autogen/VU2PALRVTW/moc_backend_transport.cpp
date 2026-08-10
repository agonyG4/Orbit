/****************************************************************************
** Meta object code from reading C++ file 'backend_transport.h'
**
** Created by: The Qt Meta Object Compiler version 69 (Qt 6.11.1)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../../../native/src/backend/backend_transport.h"
#include <QtCore/qmetatype.h>

#include <QtCore/qtmochelpers.h>

#include <memory>


#include <QtCore/qxptype_traits.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'backend_transport.h' doesn't include <QObject>."
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
struct qt_meta_tag_ZN6Astrea8Explorer6Native7Backend16BackendTransportE_t {};
} // unnamed namespace

template <> constexpr inline auto Astrea::Explorer::Native::Backend::BackendTransport::qt_create_metaobjectdata<qt_meta_tag_ZN6Astrea8Explorer6Native7Backend16BackendTransportE_t>()
{
    namespace QMC = QtMocConstants;
    QtMocHelpers::StringRefStorage qt_stringData {
        "Astrea::Explorer::Native::Backend::BackendTransport",
        "completed",
        "",
        "Astrea::Explorer::Native::Backend::BackendRequestId",
        "requestId",
        "payload",
        "failed",
        "Astrea::Explorer::Native::Backend::BackendTransportError",
        "error"
    };

    QtMocHelpers::UintData qt_methods {
        // Signal 'completed'
        QtMocHelpers::SignalData<void(Astrea::Explorer::Native::Backend::BackendRequestId, const QByteArray &)>(1, 2, QMC::AccessPublic, QMetaType::Void, {{
            { 0x80000000 | 3, 4 }, { QMetaType::QByteArray, 5 },
        }}),
        // Signal 'failed'
        QtMocHelpers::SignalData<void(Astrea::Explorer::Native::Backend::BackendRequestId, const Astrea::Explorer::Native::Backend::BackendTransportError &)>(6, 2, QMC::AccessPublic, QMetaType::Void, {{
            { 0x80000000 | 3, 4 }, { 0x80000000 | 7, 8 },
        }}),
    };
    QtMocHelpers::UintData qt_properties {
    };
    QtMocHelpers::UintData qt_enums {
    };
    return QtMocHelpers::metaObjectData<BackendTransport, qt_meta_tag_ZN6Astrea8Explorer6Native7Backend16BackendTransportE_t>(QMC::MetaObjectFlag{}, qt_stringData,
            qt_methods, qt_properties, qt_enums);
}
Q_CONSTINIT const QMetaObject Astrea::Explorer::Native::Backend::BackendTransport::staticMetaObject = { {
    QMetaObject::SuperData::link<QObject::staticMetaObject>(),
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN6Astrea8Explorer6Native7Backend16BackendTransportE_t>.stringdata,
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN6Astrea8Explorer6Native7Backend16BackendTransportE_t>.data,
    qt_static_metacall,
    nullptr,
    qt_staticMetaObjectRelocatingContent<qt_meta_tag_ZN6Astrea8Explorer6Native7Backend16BackendTransportE_t>.metaTypes,
    nullptr
} };

void Astrea::Explorer::Native::Backend::BackendTransport::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    auto *_t = static_cast<BackendTransport *>(_o);
    if (_c == QMetaObject::InvokeMetaMethod) {
        switch (_id) {
        case 0: _t->completed((*reinterpret_cast<std::add_pointer_t<Astrea::Explorer::Native::Backend::BackendRequestId>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<QByteArray>>(_a[2]))); break;
        case 1: _t->failed((*reinterpret_cast<std::add_pointer_t<Astrea::Explorer::Native::Backend::BackendRequestId>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<Astrea::Explorer::Native::Backend::BackendTransportError>>(_a[2]))); break;
        default: ;
        }
    }
    if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        switch (_id) {
        default: *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType(); break;
        case 1:
            switch (*reinterpret_cast<int*>(_a[1])) {
            default: *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType(); break;
            case 1:
                *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType::fromType< Astrea::Explorer::Native::Backend::BackendTransportError >(); break;
            }
            break;
        }
    }
    if (_c == QMetaObject::IndexOfMethod) {
        if (QtMocHelpers::indexOfMethod<void (BackendTransport::*)(Astrea::Explorer::Native::Backend::BackendRequestId , const QByteArray & )>(_a, &BackendTransport::completed, 0))
            return;
        if (QtMocHelpers::indexOfMethod<void (BackendTransport::*)(Astrea::Explorer::Native::Backend::BackendRequestId , const Astrea::Explorer::Native::Backend::BackendTransportError & )>(_a, &BackendTransport::failed, 1))
            return;
    }
}

const QMetaObject *Astrea::Explorer::Native::Backend::BackendTransport::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *Astrea::Explorer::Native::Backend::BackendTransport::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_staticMetaObjectStaticContent<qt_meta_tag_ZN6Astrea8Explorer6Native7Backend16BackendTransportE_t>.strings))
        return static_cast<void*>(this);
    return QObject::qt_metacast(_clname);
}

int Astrea::Explorer::Native::Backend::BackendTransport::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QObject::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 2)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 2;
    }
    if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 2)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 2;
    }
    return _id;
}

// SIGNAL 0
void Astrea::Explorer::Native::Backend::BackendTransport::completed(Astrea::Explorer::Native::Backend::BackendRequestId _t1, const QByteArray & _t2)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 0, nullptr, _t1, _t2);
}

// SIGNAL 1
void Astrea::Explorer::Native::Backend::BackendTransport::failed(Astrea::Explorer::Native::Backend::BackendRequestId _t1, const Astrea::Explorer::Native::Backend::BackendTransportError & _t2)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 1, nullptr, _t1, _t2);
}
QT_WARNING_POP
