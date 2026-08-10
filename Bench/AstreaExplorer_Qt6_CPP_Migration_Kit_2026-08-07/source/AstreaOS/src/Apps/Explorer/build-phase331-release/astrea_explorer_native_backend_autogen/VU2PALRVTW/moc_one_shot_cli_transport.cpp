/****************************************************************************
** Meta object code from reading C++ file 'one_shot_cli_transport.h'
**
** Created by: The Qt Meta Object Compiler version 69 (Qt 6.11.1)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../../../native/src/backend/one_shot_cli_transport.h"
#include <QtCore/qmetatype.h>

#include <QtCore/qtmochelpers.h>

#include <memory>


#include <QtCore/qxptype_traits.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'one_shot_cli_transport.h' doesn't include <QObject>."
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
struct qt_meta_tag_ZN6Astrea8Explorer6Native7Backend19OneShotCliTransportE_t {};
} // unnamed namespace

template <> constexpr inline auto Astrea::Explorer::Native::Backend::OneShotCliTransport::qt_create_metaobjectdata<qt_meta_tag_ZN6Astrea8Explorer6Native7Backend19OneShotCliTransportE_t>()
{
    namespace QMC = QtMocConstants;
    QtMocHelpers::StringRefStorage qt_stringData {
        "Astrea::Explorer::Native::Backend::OneShotCliTransport",
        "processTerminateRequested",
        "",
        "BackendRequestId",
        "requestId",
        "processKillRequested",
        "requestReleased"
    };

    QtMocHelpers::UintData qt_methods {
        // Signal 'processTerminateRequested'
        QtMocHelpers::SignalData<void(BackendRequestId)>(1, 2, QMC::AccessPublic, QMetaType::Void, {{
            { 0x80000000 | 3, 4 },
        }}),
        // Signal 'processKillRequested'
        QtMocHelpers::SignalData<void(BackendRequestId)>(5, 2, QMC::AccessPublic, QMetaType::Void, {{
            { 0x80000000 | 3, 4 },
        }}),
        // Signal 'requestReleased'
        QtMocHelpers::SignalData<void(BackendRequestId)>(6, 2, QMC::AccessPublic, QMetaType::Void, {{
            { 0x80000000 | 3, 4 },
        }}),
    };
    QtMocHelpers::UintData qt_properties {
    };
    QtMocHelpers::UintData qt_enums {
    };
    return QtMocHelpers::metaObjectData<OneShotCliTransport, qt_meta_tag_ZN6Astrea8Explorer6Native7Backend19OneShotCliTransportE_t>(QMC::MetaObjectFlag{}, qt_stringData,
            qt_methods, qt_properties, qt_enums);
}
Q_CONSTINIT const QMetaObject Astrea::Explorer::Native::Backend::OneShotCliTransport::staticMetaObject = { {
    QMetaObject::SuperData::link<BackendTransport::staticMetaObject>(),
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN6Astrea8Explorer6Native7Backend19OneShotCliTransportE_t>.stringdata,
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN6Astrea8Explorer6Native7Backend19OneShotCliTransportE_t>.data,
    qt_static_metacall,
    nullptr,
    qt_staticMetaObjectRelocatingContent<qt_meta_tag_ZN6Astrea8Explorer6Native7Backend19OneShotCliTransportE_t>.metaTypes,
    nullptr
} };

void Astrea::Explorer::Native::Backend::OneShotCliTransport::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    auto *_t = static_cast<OneShotCliTransport *>(_o);
    if (_c == QMetaObject::InvokeMetaMethod) {
        switch (_id) {
        case 0: _t->processTerminateRequested((*reinterpret_cast<std::add_pointer_t<BackendRequestId>>(_a[1]))); break;
        case 1: _t->processKillRequested((*reinterpret_cast<std::add_pointer_t<BackendRequestId>>(_a[1]))); break;
        case 2: _t->requestReleased((*reinterpret_cast<std::add_pointer_t<BackendRequestId>>(_a[1]))); break;
        default: ;
        }
    }
    if (_c == QMetaObject::IndexOfMethod) {
        if (QtMocHelpers::indexOfMethod<void (OneShotCliTransport::*)(BackendRequestId )>(_a, &OneShotCliTransport::processTerminateRequested, 0))
            return;
        if (QtMocHelpers::indexOfMethod<void (OneShotCliTransport::*)(BackendRequestId )>(_a, &OneShotCliTransport::processKillRequested, 1))
            return;
        if (QtMocHelpers::indexOfMethod<void (OneShotCliTransport::*)(BackendRequestId )>(_a, &OneShotCliTransport::requestReleased, 2))
            return;
    }
}

const QMetaObject *Astrea::Explorer::Native::Backend::OneShotCliTransport::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *Astrea::Explorer::Native::Backend::OneShotCliTransport::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_staticMetaObjectStaticContent<qt_meta_tag_ZN6Astrea8Explorer6Native7Backend19OneShotCliTransportE_t>.strings))
        return static_cast<void*>(this);
    return BackendTransport::qt_metacast(_clname);
}

int Astrea::Explorer::Native::Backend::OneShotCliTransport::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = BackendTransport::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 3)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 3;
    }
    if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 3)
            *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType();
        _id -= 3;
    }
    return _id;
}

// SIGNAL 0
void Astrea::Explorer::Native::Backend::OneShotCliTransport::processTerminateRequested(BackendRequestId _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 0, nullptr, _t1);
}

// SIGNAL 1
void Astrea::Explorer::Native::Backend::OneShotCliTransport::processKillRequested(BackendRequestId _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 1, nullptr, _t1);
}

// SIGNAL 2
void Astrea::Explorer::Native::Backend::OneShotCliTransport::requestReleased(BackendRequestId _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 2, nullptr, _t1);
}
QT_WARNING_POP
