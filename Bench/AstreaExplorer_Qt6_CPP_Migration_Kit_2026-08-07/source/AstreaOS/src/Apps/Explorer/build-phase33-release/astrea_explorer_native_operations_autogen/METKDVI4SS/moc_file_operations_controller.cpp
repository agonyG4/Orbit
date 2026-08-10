/****************************************************************************
** Meta object code from reading C++ file 'file_operations_controller.h'
**
** Created by: The Qt Meta Object Compiler version 69 (Qt 6.11.1)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../../../native/src/controllers/file_operations_controller.h"
#include <QtCore/qmetatype.h>

#include <QtCore/qtmochelpers.h>

#include <memory>


#include <QtCore/qxptype_traits.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'file_operations_controller.h' doesn't include <QObject>."
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
struct qt_meta_tag_ZN6Astrea8Explorer6Native7Backend24FileOperationsControllerE_t {};
} // unnamed namespace

template <> constexpr inline auto Astrea::Explorer::Native::Backend::FileOperationsController::qt_create_metaobjectdata<qt_meta_tag_ZN6Astrea8Explorer6Native7Backend24FileOperationsControllerE_t>()
{
    namespace QMC = QtMocConstants;
    QtMocHelpers::StringRefStorage qt_stringData {
        "Astrea::Explorer::Native::Backend::FileOperationsController",
        "clipboardChanged",
        "",
        "operationStateChanged",
        "pasteConflictChanged",
        "operationFinished",
        "Astrea::Explorer::Native::Backend::FileOperationResult",
        "result",
        "handleProgress",
        "BackendRequestId",
        "requestId",
        "FileOperationProgress",
        "progress",
        "handleFinished",
        "FileOperationResult",
        "handleFailure",
        "BackendError",
        "error",
        "setSelection",
        "paths",
        "copySelection",
        "cutSelection",
        "pasteFiles",
        "destination",
        "conflictPolicy",
        "cancelOperation",
        "isCutPending",
        "name",
        "resolvePasteConflict",
        "renamePasteConflict",
        "cancelPasteConflict",
        "clipboardFiles",
        "clipboardMode",
        "running",
        "operationProgress",
        "operationPercent",
        "operationFileName",
        "operationStatus",
        "operationError",
        "operationDestination",
        "operationDoneCount",
        "operationTotalCount",
        "operationMode",
        "pasteConflictVisible",
        "pasteConflictItems",
        "QVariantList",
        "pendingPasteRename"
    };

    QtMocHelpers::UintData qt_methods {
        // Signal 'clipboardChanged'
        QtMocHelpers::SignalData<void()>(1, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'operationStateChanged'
        QtMocHelpers::SignalData<void()>(3, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'pasteConflictChanged'
        QtMocHelpers::SignalData<void()>(4, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'operationFinished'
        QtMocHelpers::SignalData<void(const Astrea::Explorer::Native::Backend::FileOperationResult &)>(5, 2, QMC::AccessPublic, QMetaType::Void, {{
            { 0x80000000 | 6, 7 },
        }}),
        // Slot 'handleProgress'
        QtMocHelpers::SlotData<void(BackendRequestId, const FileOperationProgress &)>(8, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { 0x80000000 | 9, 10 }, { 0x80000000 | 11, 12 },
        }}),
        // Slot 'handleFinished'
        QtMocHelpers::SlotData<void(BackendRequestId, const FileOperationResult &)>(13, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { 0x80000000 | 9, 10 }, { 0x80000000 | 14, 7 },
        }}),
        // Slot 'handleFailure'
        QtMocHelpers::SlotData<void(const BackendError &)>(15, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { 0x80000000 | 16, 17 },
        }}),
        // Method 'setSelection'
        QtMocHelpers::MethodData<void(const QStringList &)>(18, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QStringList, 19 },
        }}),
        // Method 'copySelection'
        QtMocHelpers::MethodData<void()>(20, 2, QMC::AccessPublic, QMetaType::Void),
        // Method 'cutSelection'
        QtMocHelpers::MethodData<void()>(21, 2, QMC::AccessPublic, QMetaType::Void),
        // Method 'pasteFiles'
        QtMocHelpers::MethodData<BackendRequestId(const QString &, const QString &)>(22, 2, QMC::AccessPublic, 0x80000000 | 9, {{
            { QMetaType::QString, 23 }, { QMetaType::QString, 24 },
        }}),
        // Method 'pasteFiles'
        QtMocHelpers::MethodData<BackendRequestId(const QString &)>(22, 2, QMC::AccessPublic | QMC::MethodCloned, 0x80000000 | 9, {{
            { QMetaType::QString, 23 },
        }}),
        // Method 'cancelOperation'
        QtMocHelpers::MethodData<void()>(25, 2, QMC::AccessPublic, QMetaType::Void),
        // Method 'isCutPending'
        QtMocHelpers::MethodData<bool(const QString &) const>(26, 2, QMC::AccessPublic, QMetaType::Bool, {{
            { QMetaType::QString, 27 },
        }}),
        // Method 'resolvePasteConflict'
        QtMocHelpers::MethodData<void(const QString &)>(28, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 24 },
        }}),
        // Method 'renamePasteConflict'
        QtMocHelpers::MethodData<void(const QString &)>(29, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 27 },
        }}),
        // Method 'cancelPasteConflict'
        QtMocHelpers::MethodData<void()>(30, 2, QMC::AccessPublic, QMetaType::Void),
    };
    QtMocHelpers::UintData qt_properties {
        // property 'clipboardFiles'
        QtMocHelpers::PropertyData<QStringList>(31, QMetaType::QStringList, QMC::DefaultPropertyFlags, 0),
        // property 'clipboardMode'
        QtMocHelpers::PropertyData<QString>(32, QMetaType::QString, QMC::DefaultPropertyFlags, 0),
        // property 'running'
        QtMocHelpers::PropertyData<bool>(33, QMetaType::Bool, QMC::DefaultPropertyFlags, 1),
        // property 'operationProgress'
        QtMocHelpers::PropertyData<double>(34, QMetaType::Double, QMC::DefaultPropertyFlags, 1),
        // property 'operationPercent'
        QtMocHelpers::PropertyData<int>(35, QMetaType::Int, QMC::DefaultPropertyFlags, 1),
        // property 'operationFileName'
        QtMocHelpers::PropertyData<QString>(36, QMetaType::QString, QMC::DefaultPropertyFlags, 1),
        // property 'operationStatus'
        QtMocHelpers::PropertyData<QString>(37, QMetaType::QString, QMC::DefaultPropertyFlags, 1),
        // property 'operationError'
        QtMocHelpers::PropertyData<QString>(38, QMetaType::QString, QMC::DefaultPropertyFlags, 1),
        // property 'operationDestination'
        QtMocHelpers::PropertyData<QString>(39, QMetaType::QString, QMC::DefaultPropertyFlags, 1),
        // property 'operationDoneCount'
        QtMocHelpers::PropertyData<int>(40, QMetaType::Int, QMC::DefaultPropertyFlags, 1),
        // property 'operationTotalCount'
        QtMocHelpers::PropertyData<int>(41, QMetaType::Int, QMC::DefaultPropertyFlags, 1),
        // property 'operationMode'
        QtMocHelpers::PropertyData<QString>(42, QMetaType::QString, QMC::DefaultPropertyFlags, 1),
        // property 'pasteConflictVisible'
        QtMocHelpers::PropertyData<bool>(43, QMetaType::Bool, QMC::DefaultPropertyFlags, 2),
        // property 'pasteConflictItems'
        QtMocHelpers::PropertyData<QVariantList>(44, 0x80000000 | 45, QMC::DefaultPropertyFlags | QMC::EnumOrFlag, 2),
        // property 'pendingPasteRename'
        QtMocHelpers::PropertyData<QString>(46, QMetaType::QString, QMC::DefaultPropertyFlags | QMC::Writable | QMC::StdCppSet, 2),
    };
    QtMocHelpers::UintData qt_enums {
    };
    return QtMocHelpers::metaObjectData<FileOperationsController, qt_meta_tag_ZN6Astrea8Explorer6Native7Backend24FileOperationsControllerE_t>(QMC::MetaObjectFlag{}, qt_stringData,
            qt_methods, qt_properties, qt_enums);
}
Q_CONSTINIT const QMetaObject Astrea::Explorer::Native::Backend::FileOperationsController::staticMetaObject = { {
    QMetaObject::SuperData::link<QObject::staticMetaObject>(),
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN6Astrea8Explorer6Native7Backend24FileOperationsControllerE_t>.stringdata,
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN6Astrea8Explorer6Native7Backend24FileOperationsControllerE_t>.data,
    qt_static_metacall,
    nullptr,
    qt_staticMetaObjectRelocatingContent<qt_meta_tag_ZN6Astrea8Explorer6Native7Backend24FileOperationsControllerE_t>.metaTypes,
    nullptr
} };

void Astrea::Explorer::Native::Backend::FileOperationsController::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    auto *_t = static_cast<FileOperationsController *>(_o);
    if (_c == QMetaObject::InvokeMetaMethod) {
        switch (_id) {
        case 0: _t->clipboardChanged(); break;
        case 1: _t->operationStateChanged(); break;
        case 2: _t->pasteConflictChanged(); break;
        case 3: _t->operationFinished((*reinterpret_cast<std::add_pointer_t<Astrea::Explorer::Native::Backend::FileOperationResult>>(_a[1]))); break;
        case 4: _t->handleProgress((*reinterpret_cast<std::add_pointer_t<BackendRequestId>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<FileOperationProgress>>(_a[2]))); break;
        case 5: _t->handleFinished((*reinterpret_cast<std::add_pointer_t<BackendRequestId>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<FileOperationResult>>(_a[2]))); break;
        case 6: _t->handleFailure((*reinterpret_cast<std::add_pointer_t<BackendError>>(_a[1]))); break;
        case 7: _t->setSelection((*reinterpret_cast<std::add_pointer_t<QStringList>>(_a[1]))); break;
        case 8: _t->copySelection(); break;
        case 9: _t->cutSelection(); break;
        case 10: { BackendRequestId _r = _t->pasteFiles((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<QString>>(_a[2])));
            if (_a[0]) *reinterpret_cast<BackendRequestId*>(_a[0]) = std::move(_r); }  break;
        case 11: { BackendRequestId _r = _t->pasteFiles((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1])));
            if (_a[0]) *reinterpret_cast<BackendRequestId*>(_a[0]) = std::move(_r); }  break;
        case 12: _t->cancelOperation(); break;
        case 13: { bool _r = _t->isCutPending((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1])));
            if (_a[0]) *reinterpret_cast<bool*>(_a[0]) = std::move(_r); }  break;
        case 14: _t->resolvePasteConflict((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1]))); break;
        case 15: _t->renamePasteConflict((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1]))); break;
        case 16: _t->cancelPasteConflict(); break;
        default: ;
        }
    }
    if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        switch (_id) {
        default: *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType(); break;
        case 3:
            switch (*reinterpret_cast<int*>(_a[1])) {
            default: *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType(); break;
            case 0:
                *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType::fromType< Astrea::Explorer::Native::Backend::FileOperationResult >(); break;
            }
            break;
        }
    }
    if (_c == QMetaObject::IndexOfMethod) {
        if (QtMocHelpers::indexOfMethod<void (FileOperationsController::*)()>(_a, &FileOperationsController::clipboardChanged, 0))
            return;
        if (QtMocHelpers::indexOfMethod<void (FileOperationsController::*)()>(_a, &FileOperationsController::operationStateChanged, 1))
            return;
        if (QtMocHelpers::indexOfMethod<void (FileOperationsController::*)()>(_a, &FileOperationsController::pasteConflictChanged, 2))
            return;
        if (QtMocHelpers::indexOfMethod<void (FileOperationsController::*)(const Astrea::Explorer::Native::Backend::FileOperationResult & )>(_a, &FileOperationsController::operationFinished, 3))
            return;
    }
    if (_c == QMetaObject::ReadProperty) {
        void *_v = _a[0];
        switch (_id) {
        case 0: *reinterpret_cast<QStringList*>(_v) = _t->clipboardFiles(); break;
        case 1: *reinterpret_cast<QString*>(_v) = _t->clipboardMode(); break;
        case 2: *reinterpret_cast<bool*>(_v) = _t->running(); break;
        case 3: *reinterpret_cast<double*>(_v) = _t->operationProgress(); break;
        case 4: *reinterpret_cast<int*>(_v) = _t->operationPercent(); break;
        case 5: *reinterpret_cast<QString*>(_v) = _t->operationFileName(); break;
        case 6: *reinterpret_cast<QString*>(_v) = _t->operationStatus(); break;
        case 7: *reinterpret_cast<QString*>(_v) = _t->operationError(); break;
        case 8: *reinterpret_cast<QString*>(_v) = _t->operationDestination(); break;
        case 9: *reinterpret_cast<int*>(_v) = _t->operationDoneCount(); break;
        case 10: *reinterpret_cast<int*>(_v) = _t->operationTotalCount(); break;
        case 11: *reinterpret_cast<QString*>(_v) = _t->operationMode(); break;
        case 12: *reinterpret_cast<bool*>(_v) = _t->pasteConflictVisible(); break;
        case 13: *reinterpret_cast<QVariantList*>(_v) = _t->pasteConflictItems(); break;
        case 14: *reinterpret_cast<QString*>(_v) = _t->pendingPasteRename(); break;
        default: break;
        }
    }
    if (_c == QMetaObject::WriteProperty) {
        void *_v = _a[0];
        switch (_id) {
        case 14: _t->setPendingPasteRename(*reinterpret_cast<QString*>(_v)); break;
        default: break;
        }
    }
}

const QMetaObject *Astrea::Explorer::Native::Backend::FileOperationsController::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *Astrea::Explorer::Native::Backend::FileOperationsController::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_staticMetaObjectStaticContent<qt_meta_tag_ZN6Astrea8Explorer6Native7Backend24FileOperationsControllerE_t>.strings))
        return static_cast<void*>(this);
    return QObject::qt_metacast(_clname);
}

int Astrea::Explorer::Native::Backend::FileOperationsController::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QObject::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 17)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 17;
    }
    if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 17)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 17;
    }
    if (_c == QMetaObject::ReadProperty || _c == QMetaObject::WriteProperty
            || _c == QMetaObject::ResetProperty || _c == QMetaObject::BindableProperty
            || _c == QMetaObject::RegisterPropertyMetaType) {
        qt_static_metacall(this, _c, _id, _a);
        _id -= 15;
    }
    return _id;
}

// SIGNAL 0
void Astrea::Explorer::Native::Backend::FileOperationsController::clipboardChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 0, nullptr);
}

// SIGNAL 1
void Astrea::Explorer::Native::Backend::FileOperationsController::operationStateChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 1, nullptr);
}

// SIGNAL 2
void Astrea::Explorer::Native::Backend::FileOperationsController::pasteConflictChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 2, nullptr);
}

// SIGNAL 3
void Astrea::Explorer::Native::Backend::FileOperationsController::operationFinished(const Astrea::Explorer::Native::Backend::FileOperationResult & _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 3, nullptr, _t1);
}
QT_WARNING_POP
