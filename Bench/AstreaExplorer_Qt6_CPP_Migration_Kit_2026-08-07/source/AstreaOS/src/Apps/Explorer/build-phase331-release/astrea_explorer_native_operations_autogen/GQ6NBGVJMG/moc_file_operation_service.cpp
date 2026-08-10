/****************************************************************************
** Meta object code from reading C++ file 'file_operation_service.h'
**
** Created by: The Qt Meta Object Compiler version 69 (Qt 6.11.1)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../../../native/src/services/file_operation_service.h"
#include <QtCore/qmetatype.h>

#include <QtCore/qtmochelpers.h>

#include <memory>


#include <QtCore/qxptype_traits.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'file_operation_service.h' doesn't include <QObject>."
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
struct qt_meta_tag_ZN6Astrea8Explorer6Native8Services20FileOperationServiceE_t {};
} // unnamed namespace

template <> constexpr inline auto Astrea::Explorer::Native::Services::FileOperationService::qt_create_metaobjectdata<qt_meta_tag_ZN6Astrea8Explorer6Native8Services20FileOperationServiceE_t>()
{
    namespace QMC = QtMocConstants;
    QtMocHelpers::StringRefStorage qt_stringData {
        "Astrea::Explorer::Native::Services::FileOperationService",
        "progress",
        "",
        "Astrea::Explorer::Native::Backend::BackendRequestId",
        "requestId",
        "Astrea::Explorer::Native::Backend::FileOperationProgress",
        "finished",
        "Astrea::Explorer::Native::Backend::FileOperationResult",
        "result",
        "failed",
        "Astrea::Explorer::Native::Backend::BackendError",
        "error",
        "handleProgress",
        "Backend::BackendRequestId",
        "Backend::FileOperationProgress",
        "handleFinished",
        "Backend::FileOperationResult",
        "handleFailure",
        "Backend::BackendError"
    };

    QtMocHelpers::UintData qt_methods {
        // Signal 'progress'
        QtMocHelpers::SignalData<void(Astrea::Explorer::Native::Backend::BackendRequestId, const Astrea::Explorer::Native::Backend::FileOperationProgress &)>(1, 2, QMC::AccessPublic, QMetaType::Void, {{
            { 0x80000000 | 3, 4 }, { 0x80000000 | 5, 1 },
        }}),
        // Signal 'finished'
        QtMocHelpers::SignalData<void(Astrea::Explorer::Native::Backend::BackendRequestId, const Astrea::Explorer::Native::Backend::FileOperationResult &)>(6, 2, QMC::AccessPublic, QMetaType::Void, {{
            { 0x80000000 | 3, 4 }, { 0x80000000 | 7, 8 },
        }}),
        // Signal 'failed'
        QtMocHelpers::SignalData<void(const Astrea::Explorer::Native::Backend::BackendError &)>(9, 2, QMC::AccessPublic, QMetaType::Void, {{
            { 0x80000000 | 10, 11 },
        }}),
        // Slot 'handleProgress'
        QtMocHelpers::SlotData<void(Backend::BackendRequestId, const Backend::FileOperationProgress &)>(12, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { 0x80000000 | 13, 4 }, { 0x80000000 | 14, 1 },
        }}),
        // Slot 'handleFinished'
        QtMocHelpers::SlotData<void(Backend::BackendRequestId, const Backend::FileOperationResult &)>(15, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { 0x80000000 | 13, 4 }, { 0x80000000 | 16, 8 },
        }}),
        // Slot 'handleFailure'
        QtMocHelpers::SlotData<void(const Backend::BackendError &)>(17, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { 0x80000000 | 18, 11 },
        }}),
    };
    QtMocHelpers::UintData qt_properties {
    };
    QtMocHelpers::UintData qt_enums {
    };
    return QtMocHelpers::metaObjectData<FileOperationService, qt_meta_tag_ZN6Astrea8Explorer6Native8Services20FileOperationServiceE_t>(QMC::MetaObjectFlag{}, qt_stringData,
            qt_methods, qt_properties, qt_enums);
}
Q_CONSTINIT const QMetaObject Astrea::Explorer::Native::Services::FileOperationService::staticMetaObject = { {
    QMetaObject::SuperData::link<QObject::staticMetaObject>(),
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN6Astrea8Explorer6Native8Services20FileOperationServiceE_t>.stringdata,
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN6Astrea8Explorer6Native8Services20FileOperationServiceE_t>.data,
    qt_static_metacall,
    nullptr,
    qt_staticMetaObjectRelocatingContent<qt_meta_tag_ZN6Astrea8Explorer6Native8Services20FileOperationServiceE_t>.metaTypes,
    nullptr
} };

void Astrea::Explorer::Native::Services::FileOperationService::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    auto *_t = static_cast<FileOperationService *>(_o);
    if (_c == QMetaObject::InvokeMetaMethod) {
        switch (_id) {
        case 0: _t->progress((*reinterpret_cast<std::add_pointer_t<Astrea::Explorer::Native::Backend::BackendRequestId>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<Astrea::Explorer::Native::Backend::FileOperationProgress>>(_a[2]))); break;
        case 1: _t->finished((*reinterpret_cast<std::add_pointer_t<Astrea::Explorer::Native::Backend::BackendRequestId>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<Astrea::Explorer::Native::Backend::FileOperationResult>>(_a[2]))); break;
        case 2: _t->failed((*reinterpret_cast<std::add_pointer_t<Astrea::Explorer::Native::Backend::BackendError>>(_a[1]))); break;
        case 3: _t->handleProgress((*reinterpret_cast<std::add_pointer_t<Backend::BackendRequestId>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<Backend::FileOperationProgress>>(_a[2]))); break;
        case 4: _t->handleFinished((*reinterpret_cast<std::add_pointer_t<Backend::BackendRequestId>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<Backend::FileOperationResult>>(_a[2]))); break;
        case 5: _t->handleFailure((*reinterpret_cast<std::add_pointer_t<Backend::BackendError>>(_a[1]))); break;
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
                *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType::fromType< Astrea::Explorer::Native::Backend::FileOperationProgress >(); break;
            }
            break;
        case 1:
            switch (*reinterpret_cast<int*>(_a[1])) {
            default: *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType(); break;
            case 1:
                *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType::fromType< Astrea::Explorer::Native::Backend::FileOperationResult >(); break;
            }
            break;
        case 2:
            switch (*reinterpret_cast<int*>(_a[1])) {
            default: *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType(); break;
            case 0:
                *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType::fromType< Astrea::Explorer::Native::Backend::BackendError >(); break;
            }
            break;
        }
    }
    if (_c == QMetaObject::IndexOfMethod) {
        if (QtMocHelpers::indexOfMethod<void (FileOperationService::*)(Astrea::Explorer::Native::Backend::BackendRequestId , const Astrea::Explorer::Native::Backend::FileOperationProgress & )>(_a, &FileOperationService::progress, 0))
            return;
        if (QtMocHelpers::indexOfMethod<void (FileOperationService::*)(Astrea::Explorer::Native::Backend::BackendRequestId , const Astrea::Explorer::Native::Backend::FileOperationResult & )>(_a, &FileOperationService::finished, 1))
            return;
        if (QtMocHelpers::indexOfMethod<void (FileOperationService::*)(const Astrea::Explorer::Native::Backend::BackendError & )>(_a, &FileOperationService::failed, 2))
            return;
    }
}

const QMetaObject *Astrea::Explorer::Native::Services::FileOperationService::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *Astrea::Explorer::Native::Services::FileOperationService::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_staticMetaObjectStaticContent<qt_meta_tag_ZN6Astrea8Explorer6Native8Services20FileOperationServiceE_t>.strings))
        return static_cast<void*>(this);
    return QObject::qt_metacast(_clname);
}

int Astrea::Explorer::Native::Services::FileOperationService::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QObject::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 6)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 6;
    }
    if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 6)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 6;
    }
    return _id;
}

// SIGNAL 0
void Astrea::Explorer::Native::Services::FileOperationService::progress(Astrea::Explorer::Native::Backend::BackendRequestId _t1, const Astrea::Explorer::Native::Backend::FileOperationProgress & _t2)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 0, nullptr, _t1, _t2);
}

// SIGNAL 1
void Astrea::Explorer::Native::Services::FileOperationService::finished(Astrea::Explorer::Native::Backend::BackendRequestId _t1, const Astrea::Explorer::Native::Backend::FileOperationResult & _t2)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 1, nullptr, _t1, _t2);
}

// SIGNAL 2
void Astrea::Explorer::Native::Services::FileOperationService::failed(const Astrea::Explorer::Native::Backend::BackendError & _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 2, nullptr, _t1);
}
QT_WARNING_POP
