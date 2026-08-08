cmake_minimum_required(VERSION 3.21)

project(AstreaExplorerNative LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Qt6 6.11 REQUIRED COMPONENTS Core Gui Qml Quick QuickControls2 Test)
qt_standard_project_setup(REQUIRES 6.11)

enable_testing()

set(ASTREA_EXPLORER_BIN "${CMAKE_BINARY_DIR}/astrea-explorer")

add_library(astrea_explorer_native_backend STATIC
    src/backend/backend_types.h
    src/backend/backend_transport.h
    src/backend/one_shot_cli_transport.h
    src/backend/one_shot_cli_transport.cpp
    src/backend/rust_backend_client.h
    src/backend/rust_backend_client.cpp
    src/backend/fake_backend_client.h
    src/backend/fake_backend_client.cpp
    src/models/directory_model.h
    src/models/directory_model.cpp
)

target_include_directories(astrea_explorer_native_backend PUBLIC
    src
)

target_link_libraries(astrea_explorer_native_backend PUBLIC
    Qt6::Core
)

add_library(astrea_explorer_native_settings STATIC
    src/services/settings_service.h
    src/services/settings_service.cpp
)

target_include_directories(astrea_explorer_native_settings PUBLIC
    src
)

target_link_libraries(astrea_explorer_native_settings PUBLIC
    Qt6::Core
)

add_library(astrea_explorer_native_clipboard STATIC
    src/services/clipboard_service.h
    src/services/clipboard_service.cpp
)

target_include_directories(astrea_explorer_native_clipboard PUBLIC
    src
)

target_link_libraries(astrea_explorer_native_clipboard PUBLIC
    Qt6::Core
    Qt6::Gui
)

add_library(astrea_explorer_native_launch STATIC
    src/services/launch_service.h
    src/services/launch_service.cpp
)

target_include_directories(astrea_explorer_native_launch PUBLIC
    src
)

target_link_libraries(astrea_explorer_native_launch PUBLIC
    Qt6::Core
)

qt_add_executable(astrea-explorer
    src/main.cpp
    src/explorer_application.cpp
    src/explorer_application.h
)

target_include_directories(astrea-explorer PRIVATE
    src
)

target_link_libraries(astrea-explorer PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Qml
    Qt6::Quick
    Qt6::QuickControls2
    astrea_explorer_native_backend
)

qt_add_qml_module(astrea-explorer
    URI Astrea.Explorer.Native
    VERSION 1.0
    QML_FILES
        qml/NativeBootstrap.qml
)

qt_add_executable(tst_backend_client
    tests/tst_backend_client.cpp
)

target_include_directories(tst_backend_client PRIVATE
    src
)

target_link_libraries(tst_backend_client PRIVATE
    Qt6::Core
    Qt6::Test
    astrea_explorer_native_backend
)

add_test(NAME backend_client COMMAND tst_backend_client)

qt_add_executable(tst_directory_model
    tests/tst_directory_model.cpp
)

target_include_directories(tst_directory_model PRIVATE
    src
)

target_link_libraries(tst_directory_model PRIVATE
    Qt6::Core
    Qt6::Test
    astrea_explorer_native_backend
)

add_test(NAME directory_model COMMAND tst_directory_model)

add_library(astrea_explorer_native_navigation STATIC
    src/services/directory_watch_service.h
    src/services/directory_watch_service.cpp
    src/controllers/navigation_controller.h
    src/controllers/navigation_controller.cpp
    src/controllers/selection_controller.h
    src/controllers/selection_controller.cpp
    src/controllers/recent_controller.h
    src/controllers/recent_controller.cpp
    src/controllers/preview_controller.h
    src/controllers/preview_controller.cpp
    src/controllers/device_controller.h
    src/controllers/device_controller.cpp
    src/controllers/app_state_facade.h
    src/controllers/app_state_facade.cpp
)

target_include_directories(astrea_explorer_native_navigation PUBLIC
    src
)

target_link_libraries(astrea_explorer_native_navigation PUBLIC
    Qt6::Core
    astrea_explorer_native_backend
)

target_link_libraries(astrea-explorer PRIVATE
    astrea_explorer_native_navigation
    astrea_explorer_native_settings
)

qt_add_executable(tst_navigation_controller
    tests/tst_navigation_controller.cpp
)

target_include_directories(tst_navigation_controller PRIVATE
    src
)

target_link_libraries(tst_navigation_controller PRIVATE
    Qt6::Core
    Qt6::Test
    astrea_explorer_native_navigation
)

add_test(NAME navigation_controller COMMAND tst_navigation_controller)

qt_add_executable(tst_selection_controller
    tests/tst_selection_controller.cpp
)

target_include_directories(tst_selection_controller PRIVATE
    src
)

target_link_libraries(tst_selection_controller PRIVATE
    Qt6::Core
    Qt6::Qml
    Qt6::Test
    astrea_explorer_native_navigation
)

add_test(NAME selection_controller COMMAND tst_selection_controller)

qt_add_executable(tst_settings_service
    tests/tst_settings_service.cpp
)

target_include_directories(tst_settings_service PRIVATE
    src
)

target_link_libraries(tst_settings_service PRIVATE
    Qt6::Core
    Qt6::Test
    astrea_explorer_native_settings
)

add_test(NAME settings_service COMMAND tst_settings_service)

qt_add_executable(tst_clipboard_service
    tests/tst_clipboard_service.cpp
)

target_include_directories(tst_clipboard_service PRIVATE
    src
)

target_link_libraries(tst_clipboard_service PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Test
    astrea_explorer_native_clipboard
)

add_test(NAME clipboard_service COMMAND tst_clipboard_service)

qt_add_executable(tst_launch_service
    tests/tst_launch_service.cpp
)

target_include_directories(tst_launch_service PRIVATE
    src
)

target_link_libraries(tst_launch_service PRIVATE
    Qt6::Core
    Qt6::Test
    astrea_explorer_native_launch
)

add_test(NAME launch_service COMMAND tst_launch_service)

qt_add_executable(tst_recent_controller
    tests/tst_recent_controller.cpp
)

target_include_directories(tst_recent_controller PRIVATE
    src
)

target_link_libraries(tst_recent_controller PRIVATE
    Qt6::Core
    Qt6::Test
    astrea_explorer_native_navigation
)

add_test(NAME recent_controller COMMAND tst_recent_controller)

qt_add_executable(tst_preview_controller
    tests/tst_preview_controller.cpp
)

target_include_directories(tst_preview_controller PRIVATE
    src
)

target_link_libraries(tst_preview_controller PRIVATE
    Qt6::Core
    Qt6::Test
    astrea_explorer_native_navigation
)

add_test(NAME preview_controller COMMAND tst_preview_controller)

qt_add_executable(tst_device_controller
    tests/tst_device_controller.cpp
)

target_include_directories(tst_device_controller PRIVATE
    src
)

target_link_libraries(tst_device_controller PRIVATE
    Qt6::Core
    Qt6::Test
    astrea_explorer_native_navigation
)

add_test(NAME device_controller COMMAND tst_device_controller)

qt_add_executable(tst_app_state_facade
    tests/tst_app_state_facade.cpp
)

target_include_directories(tst_app_state_facade PRIVATE
    src
)

target_link_libraries(tst_app_state_facade PRIVATE
    Qt6::Core
    Qt6::Test
    astrea_explorer_native_navigation
    astrea_explorer_native_settings
)

add_test(NAME app_state_facade COMMAND tst_app_state_facade)

set(ASTREA_EXPLORER_RUST_ROOT
    "${CMAKE_CURRENT_SOURCE_DIR}/../../../Core/bridge/apps/explorer")

qt_add_executable(tst_shadow_parity
    tests/parity/fixture_tree.h
    tests/parity/fixture_tree.cpp
    tests/parity/legacy_oracle.h
    tests/parity/legacy_oracle.cpp
    tests/parity/native_oracle.h
    tests/parity/native_oracle.cpp
    tests/parity/parity_snapshot.h
    tests/parity/parity_snapshot.cpp
    tests/tst_shadow_parity.cpp
)

target_include_directories(tst_shadow_parity PRIVATE
    src
    tests
)

target_compile_definitions(tst_shadow_parity PRIVATE
    ASTREA_EXPLORER_PARITY_BACKEND="${ASTREA_EXPLORER_RUST_ROOT}/target/debug/explorer_backend"
    ASTREA_EXPLORER_PARITY_HELPER="${CMAKE_CURRENT_SOURCE_DIR}/../explorer_helper.py"
)

target_link_libraries(tst_shadow_parity PRIVATE
    Qt6::Core
    Qt6::Test
    astrea_explorer_native_navigation
)

add_test(NAME shadow_parity COMMAND tst_shadow_parity)

add_subdirectory(tests)
