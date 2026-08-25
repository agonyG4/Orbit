qt_add_executable(tst_native_bootstrap
    tst_native_bootstrap.cpp
)

target_link_libraries(tst_native_bootstrap PRIVATE
    Qt6::Core
    Qt6::Test
)

target_compile_definitions(tst_native_bootstrap PRIVATE
    ASTREA_EXPLORER_BIN="${ASTREA_EXPLORER_BIN}"
    ASTREA_EXPLORER_RUNTIME_ROOT="${ASTREA_EXPLORER_RUNTIME_ROOT}"
)

add_test(NAME native_bootstrap COMMAND tst_native_bootstrap)

qt_add_executable(tst_real_main_qml
    tst_real_main_qml.cpp
)

target_link_libraries(tst_real_main_qml PRIVATE
    Qt6::Core
    Qt6::Test
)

target_compile_definitions(tst_real_main_qml PRIVATE
    ASTREA_EXPLORER_BIN="${ASTREA_EXPLORER_BIN}"
    ASTREA_EXPLORER_RUNTIME_ROOT="${ASTREA_EXPLORER_RUNTIME_ROOT}"
)

add_test(NAME real_main_qml COMMAND tst_real_main_qml)

qt_add_executable(tst_operation_progress_presenter
    tst_operation_progress_presenter.cpp
)

target_link_libraries(tst_operation_progress_presenter PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Qml
    Qt6::Quick
    Qt6::Test
)

target_compile_definitions(tst_operation_progress_presenter PRIVATE
    ASTREA_EXPLORER_RUNTIME_ROOT="${ASTREA_EXPLORER_RUNTIME_ROOT}"
)

add_test(NAME operation_progress_presenter COMMAND tst_operation_progress_presenter)
set_tests_properties(operation_progress_presenter PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
)

qt_add_executable(tst_app_state_compatibility
    tst_app_state_compatibility.cpp
)

target_include_directories(tst_app_state_compatibility PRIVATE
    ../../src
)

target_link_libraries(tst_app_state_compatibility PRIVATE
    Qt6::Core
    Qt6::Qml
    Qt6::Test
    astrea_explorer_native_navigation
    astrea_explorer_native_operations
)

target_compile_definitions(tst_app_state_compatibility PRIVATE
    ASTREA_EXPLORER_RUNTIME_ROOT="${ASTREA_EXPLORER_RUNTIME_ROOT}"
    ASTREA_EXPLORER_NATIVE_SOURCE_ROOT="${CMAKE_CURRENT_SOURCE_DIR}/../../src"
)

add_test(NAME app_state_compatibility COMMAND tst_app_state_compatibility)

qt_add_executable(tst_recent_persistence
    tst_recent_persistence.cpp
)

target_link_libraries(tst_recent_persistence PRIVATE
    Qt6::Core
    Qt6::Qml
    Qt6::Test
)

target_compile_definitions(tst_recent_persistence PRIVATE
    ASTREA_EXPLORER_RUNTIME_ROOT="${ASTREA_EXPLORER_RUNTIME_ROOT}"
)

add_test(NAME recent_persistence COMMAND tst_recent_persistence)

qt_add_executable(tst_qml_warning_lifetime
    tst_qml_warning_lifetime.cpp
)

target_link_libraries(tst_qml_warning_lifetime PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Qml
    Qt6::Test
)

add_test(NAME qml_warning_lifetime COMMAND tst_qml_warning_lifetime)
set_tests_properties(qml_warning_lifetime PROPERTIES
    ENVIRONMENT "ASTREA_EXPLORER_NATIVE_SOURCE_ROOT=${CMAKE_CURRENT_SOURCE_DIR}/../../src"
)
