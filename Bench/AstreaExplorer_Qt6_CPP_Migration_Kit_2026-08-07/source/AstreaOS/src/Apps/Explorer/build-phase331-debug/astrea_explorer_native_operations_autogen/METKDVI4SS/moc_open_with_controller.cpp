/****************************************************************************
** Meta object code from reading C++ file 'open_with_controller.h'
**
** Created by: The Qt Meta Object Compiler version 69 (Qt 6.11.1)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../../../native/src/controllers/open_with_controller.h"
#include <QtCore/qmetatype.h>

#include <QtCore/qtmochelpers.h>

#include <memory>


#include <QtCore/qxptype_traits.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'open_with_controller.h' doesn't include <QObject>."
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
struct qt_meta_tag_ZN6Astrea8Explorer6Native7Backend18OpenWithControllerE_t {};
} // unnamed namespace

template <> constexpr inline auto Astrea::Explorer::Native::Backend::OpenWithController::qt_create_metaobjectdata<qt_meta_tag_ZN6Astrea8Explorer6Native7Backend18OpenWithControllerE_t>()
{
    namespace QMC = QtMocConstants;
    QtMocHelpers::StringRefStorage qt_stringData {
        "Astrea::Explorer::Native::Backend::OpenWithController",
        "applicationsChanged",
        "",
        "selectionChanged",
        "busyChanged",
        "errorChanged",
        "launched",
        "applicationId",
        "discover",
        "path",
        "selectApplication",
        "id",
        "launchSelected",
        "applicationList",
        "QVariantList",
        "selectedApplicationId",
        "busy",
        "error"
    };

    QtMocHelpers::UintData qt_methods {
        // Signal 'applicationsChanged'
        QtMocHelpers::SignalData<void()>(1, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'selectionChanged'
        QtMocHelpers::SignalData<void()>(3, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'busyChanged'
        QtMocHelpers::SignalData<void()>(4, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'errorChanged'
        QtMocHelpers::SignalData<void()>(5, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'launched'
        QtMocHelpers::SignalData<void(const QString &)>(6, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 7 },
        }}),
        // Method 'discover'
        QtMocHelpers::MethodData<void(const QString &)>(8, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 9 },
        }}),
        // Method 'selectApplication'
        QtMocHelpers::MethodData<void(const QString &)>(10, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 11 },
        }}),
        // Method 'launchSelected'
        QtMocHelpers::MethodData<bool(const QString &)>(12, 2, QMC::AccessPublic, QMetaType::Bool, {{
            { QMetaType::QString, 9 },
        }}),
    };
    QtMocHelpers::UintData qt_properties {
        // property 'applicationList'
        QtMocHelpers::PropertyData<QVariantList>(13, 0x80000000 | 14, QMC::DefaultPropertyFlags | QMC::EnumOrFlag, 0),
        // property 'selectedApplicationId'
        QtMocHelpers::PropertyData<QString>(15, QMetaType::QString, QMC::DefaultPropertyFlags, 1),
        // property 'busy'
        QtMocHelpers::PropertyData<bool>(16, QMetaType::Bool, QMC::DefaultPropertyFlags, 2),
        // property 'error'
        QtMocHelpers::PropertyData<QString>(17, QMetaType::QString, QMC::DefaultPropertyFlags, 3),
    };
    QtMocHelpers::UintData qt_enums {
    };
    return QtMocHelpers::metaObjectData<OpenWithController, qt_meta_tag_ZN6Astrea8Explorer6Native7Backend18OpenWithControllerE_t>(QMC::MetaObjectFlag{}, qt_stringData,
            qt_methods, qt_properties, qt_enums);
}
Q_CONSTINIT const QMetaObject Astrea::Explorer::Native::Backend::OpenWithController::staticMetaObject = { {
    QMetaObject::SuperData::link<QObject::staticMetaObject>(),
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN6Astrea8Explorer6Native7Backend18OpenWithControllerE_t>.stringdata,
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN6Astrea8Explorer6Native7Backend18OpenWithControllerE_t>.data,
    qt_static_metacall,
    nullptr,
    qt_staticMetaObjectRelocatingContent<qt_meta_tag_ZN6Astrea8Explorer6Native7Backend18OpenWithControllerE_t>.metaTypes,
    nullptr
} };

void Astrea::Explorer::Native::Backend::OpenWithController::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    auto *_t = static_cast<OpenWithController *>(_o);
    if (_c == QMetaObject::InvokeMetaMethod) {
        switch (_id) {
        case 0: _t->applicationsChanged(); break;
        case 1: _t->selectionChanged(); break;
        case 2: _t->busyChanged(); break;
        case 3: _t->errorChanged(); break;
        case 4: _t->launched((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1]))); break;
        case 5: _t->discover((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1]))); break;
        case 6: _t->selectApplication((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1]))); break;
        case 7: { bool _r = _t->launchSelected((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1])));
            if (_a[0]) *reinterpret_cast<bool*>(_a[0]) = std::move(_r); }  break;
        default: ;
        }
    }
    if (_c == QMetaObject::IndexOfMethod) {
        if (QtMocHelpers::indexOfMethod<void (OpenWithController::*)()>(_a, &OpenWithController::applicationsChanged, 0))
            return;
        if (QtMocHelpers::indexOfMethod<void (OpenWithController::*)()>(_a, &OpenWithController::selectionChanged, 1))
            return;
        if (QtMocHelpers::indexOfMethod<void (OpenWithController::*)()>(_a, &OpenWithController::busyChanged, 2))
            return;
        if (QtMocHelpers::indexOfMethod<void (OpenWithController::*)()>(_a, &OpenWithController::errorChanged, 3))
            return;
        if (QtMocHelpers::indexOfMethod<void (OpenWithController::*)(const QString & )>(_a, &OpenWithController::launched, 4))
            return;
    }
    if (_c == QMetaObject::ReadProperty) {
        void *_v = _a[0];
        switch (_id) {
        case 0: *reinterpret_cast<QVariantList*>(_v) = _t->applicationList(); break;
        case 1: *reinterpret_cast<QString*>(_v) = _t->selectedApplicationId(); break;
        case 2: *reinterpret_cast<bool*>(_v) = _t->busy(); break;
        case 3: *reinterpret_cast<QString*>(_v) = _t->error(); break;
        default: break;
        }
    }
}

const QMetaObject *Astrea::Explorer::Native::Backend::OpenWithController::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *Astrea::Explorer::Native::Backend::OpenWithController::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_staticMetaObjectStaticContent<qt_meta_tag_ZN6Astrea8Explorer6Native7Backend18OpenWithControllerE_t>.strings))
        return static_cast<void*>(this);
    return QObject::qt_metacast(_clname);
}

int Astrea::Explorer::Native::Backend::OpenWithController::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
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
            *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType();
        _id -= 8;
    }
    if (_c == QMetaObject::ReadProperty || _c == QMetaObject::WriteProperty
            || _c == QMetaObject::ResetProperty || _c == QMetaObject::BindableProperty
            || _c == QMetaObject::RegisterPropertyMetaType) {
        qt_static_metacall(this, _c, _id, _a);
        _id -= 4;
    }
    return _id;
}

// SIGNAL 0
void Astrea::Explorer::Native::Backend::OpenWithController::applicationsChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 0, nullptr);
}

// SIGNAL 1
void Astrea::Explorer::Native::Backend::OpenWithController::selectionChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 1, nullptr);
}

// SIGNAL 2
void Astrea::Explorer::Native::Backend::OpenWithController::busyChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 2, nullptr);
}

// SIGNAL 3
void Astrea::Explorer::Native::Backend::OpenWithController::errorChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 3, nullptr);
}

// SIGNAL 4
void Astrea::Explorer::Native::Backend::OpenWithController::launched(const QString & _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 4, nullptr, _t1);
}
QT_WARNING_POP
