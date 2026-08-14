cmake_minimum_required(VERSION 3.21)

project(AstreaExplorerNative LANGUAGES CXX)

include(GNUInstallDirs)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Qt6 6.11 REQUIRED COMPONENTS Core Gui Qml Quick QuickControls2 Test)
qt_standard_project_setup(REQUIRES 6.11)

enable_testing()

set(ASTREA_EXPLORER_BIN "${CMAKE_BINARY_DIR}/astrea-explorer")
set(ASTREA_EXPLORER_RUNTIME_ROOT
    "${CMAKE_CURRENT_SOURCE_DIR}/../../../../src")
set(ASTREA_EXPLORER_RUST_ROOT
    "${CMAKE_CURRENT_SOURCE_DIR}/../../../Core/bridge/apps/explorer")
set(ASTREA_EXPLORER_PORTAL_ROOT
    "${CMAKE_CURRENT_SOURCE_DIR}/../../../System/portal")
set(ASTREA_EXPLORER_LAUNCH_PROGRAM
    "${CMAKE_CURRENT_SOURCE_DIR}/../../../bin/astrea-launch")

find_program(ASTREA_CARGO_EXECUTABLE cargo REQUIRED)

if (CMAKE_BUILD_TYPE STREQUAL "Release" OR CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo")
    set(ASTREA_RUST_PROFILE_ARG --release)
    set(ASTREA_RUST_PROFILE_DIR release)
else()
    set(ASTREA_RUST_PROFILE_ARG)
    set(ASTREA_RUST_PROFILE_DIR debug)
endif()

set(ASTREA_RUST_TARGET_DIR "${CMAKE_CURRENT_BINARY_DIR}/cargo-target")
set(ASTREA_EXPLORER_BACKEND_ARTIFACT
    "${ASTREA_RUST_TARGET_DIR}/${ASTREA_RUST_PROFILE_DIR}/explorer_backend")
set(ASTREA_EXPLORER_PORTAL_ARTIFACT
    "${ASTREA_RUST_TARGET_DIR}/${ASTREA_RUST_PROFILE_DIR}/astrea-filechooser-portal")

file(GLOB_RECURSE ASTREA_EXPLORER_RUST_SOURCES CONFIGURE_DEPENDS
    "${ASTREA_EXPLORER_RUST_ROOT}/*.rs"
    "${ASTREA_EXPLORER_RUST_ROOT}/Cargo.toml"
    "${ASTREA_EXPLORER_RUST_ROOT}/Cargo.lock")
file(GLOB_RECURSE ASTREA_EXPLORER_PORTAL_RUST_SOURCES CONFIGURE_DEPENDS
    "${ASTREA_EXPLORER_PORTAL_ROOT}/*.rs"
    "${ASTREA_EXPLORER_PORTAL_ROOT}/Cargo.toml"
    "${ASTREA_EXPLORER_PORTAL_ROOT}/Cargo.lock")

add_custom_command(
    OUTPUT "${ASTREA_EXPLORER_BACKEND_ARTIFACT}"
    COMMAND "${CMAKE_COMMAND}" -E env
        "CARGO_TARGET_DIR=${ASTREA_RUST_TARGET_DIR}"
        "${ASTREA_CARGO_EXECUTABLE}" build
        --manifest-path "${ASTREA_EXPLORER_RUST_ROOT}/Cargo.toml"
        ${ASTREA_RUST_PROFILE_ARG}
    WORKING_DIRECTORY "${ASTREA_EXPLORER_RUST_ROOT}"
    DEPENDS ${ASTREA_EXPLORER_RUST_SOURCES}
    COMMENT "Building the Astrea Explorer Rust backend"
    VERBATIM)

add_custom_command(
    OUTPUT "${ASTREA_EXPLORER_PORTAL_ARTIFACT}"
    COMMAND "${CMAKE_COMMAND}" -E env
        "CARGO_TARGET_DIR=${ASTREA_RUST_TARGET_DIR}"
        "${ASTREA_CARGO_EXECUTABLE}" build
        --manifest-path "${ASTREA_EXPLORER_PORTAL_ROOT}/Cargo.toml"
        ${ASTREA_RUST_PROFILE_ARG}
    WORKING_DIRECTORY "${ASTREA_EXPLORER_PORTAL_ROOT}"
    DEPENDS ${ASTREA_EXPLORER_PORTAL_RUST_SOURCES}
    COMMENT "Building the Astrea FileChooser portal"
    VERBATIM)

add_custom_target(astrea_explorer_rust_components
    DEPENDS
        "${ASTREA_EXPLORER_BACKEND_ARTIFACT}"
        "${ASTREA_EXPLORER_PORTAL_ARTIFACT}")

if (NOT EXISTS "${ASTREA_EXPLORER_LAUNCH_PROGRAM}")
    message(FATAL_ERROR
        "Required Astrea launch provider is missing: ${ASTREA_EXPLORER_LAUNCH_PROGRAM}")
endif()

add_library(astrea_explorer_native_backend STATIC
    src/backend/backend_types.h
    src/backend/backend_transport.h
    src/backend/persistent_worker_transport.h
    src/backend/persistent_worker_transport.cpp
    src/backend/one_shot_cli_transport.h
    src/backend/one_shot_cli_transport.cpp
    src/backend/rust_backend_client.h
    src/backend/rust_backend_client.cpp
    src/backend/fake_backend_client.h
    src/backend/fake_backend_client.cpp
    src/models/directory_model.h
    src/models/directory_model.cpp
)

add_library(astrea_explorer_native_runtime STATIC
    src/runtime/explorer_runtime_paths.h
    src/runtime/explorer_runtime_paths.cpp
)

target_include_directories(astrea_explorer_native_runtime PUBLIC
    src
)

target_link_libraries(astrea_explorer_native_runtime PUBLIC
    Qt6::Core
)

add_library(astrea_explorer_native_operations STATIC
    src/controllers/file_operations_controller.h
    src/controllers/file_operations_controller.cpp
    src/controllers/open_with_controller.h
    src/controllers/open_with_controller.cpp
    src/controllers/portal_controller.h
    src/controllers/portal_controller.cpp
    src/services/file_operation_service.h
    src/services/file_operation_service.cpp
    src/services/filesystem_service.h
    src/services/filesystem_service.cpp
    src/services/desktop_file_id.h
    src/services/desktop_file_id.cpp
    src/services/mime_apps_service.h
    src/services/mime_apps_service.cpp
    src/services/wallpaper_service.h
    src/services/wallpaper_service.cpp
)

target_include_directories(astrea_explorer_native_operations PUBLIC
    src
)

target_link_libraries(astrea_explorer_native_operations PUBLIC
    Qt6::Core
    astrea_explorer_native_backend
    astrea_explorer_native_launch
    astrea_explorer_native_clipboard
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

add_dependencies(astrea-explorer astrea_explorer_rust_components)

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
    astrea_explorer_native_runtime
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

qt_add_executable(tst_persistent_worker_transport
    tests/tst_persistent_worker_transport.cpp
)

target_include_directories(tst_persistent_worker_transport PRIVATE
    src
)

target_link_libraries(tst_persistent_worker_transport PRIVATE
    Qt6::Core
    Qt6::Test
    astrea_explorer_native_backend
)

add_test(NAME persistent_worker_transport COMMAND tst_persistent_worker_transport)

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

qt_add_executable(tst_runtime_paths
    tests/tst_runtime_paths.cpp
)

target_include_directories(tst_runtime_paths PRIVATE
    src
)

target_link_libraries(tst_runtime_paths PRIVATE
    Qt6::Core
    Qt6::Test
    astrea_explorer_native_runtime
)

add_test(NAME runtime_paths COMMAND tst_runtime_paths)

add_library(astrea_explorer_native_recent STATIC
    src/services/recent_store.h
    src/services/recent_store.cpp
    src/controllers/recent_controller.h
    src/controllers/recent_controller.cpp
)

target_include_directories(astrea_explorer_native_recent PUBLIC
    src
)

target_link_libraries(astrea_explorer_native_recent PUBLIC
    Qt6::Core
    astrea_explorer_native_backend
    astrea_explorer_native_operations
)

add_library(astrea_explorer_native_navigation STATIC
    src/services/directory_watch_service.h
    src/services/directory_watch_service.cpp
    src/controllers/navigation_controller.h
    src/controllers/navigation_controller.cpp
    src/controllers/selection_controller.h
    src/controllers/selection_controller.cpp
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
    astrea_explorer_native_settings
    astrea_explorer_native_operations
    astrea_explorer_native_recent
)

target_link_libraries(astrea-explorer PRIVATE
    astrea_explorer_native_navigation
    astrea_explorer_native_settings
    astrea_explorer_native_operations
    astrea_explorer_native_clipboard
)

qt_add_executable(tst_file_operations_controller
    tests/tst_file_operations_controller.cpp
)

target_include_directories(tst_file_operations_controller PRIVATE
    src
)

target_link_libraries(tst_file_operations_controller PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Test
    astrea_explorer_native_operations
)

add_test(NAME file_operations_controller COMMAND tst_file_operations_controller)

qt_add_executable(tst_open_with_controller
    tests/tst_open_with_controller.cpp
)

target_include_directories(tst_open_with_controller PRIVATE
    src
)

target_link_libraries(tst_open_with_controller PRIVATE
    Qt6::Core
    Qt6::Test
    astrea_explorer_native_operations
)

add_test(NAME open_with_controller COMMAND tst_open_with_controller)

qt_add_executable(tst_mime_apps_service
    tests/tst_mime_apps_service.cpp
)

target_include_directories(tst_mime_apps_service PRIVATE
    src
)

target_link_libraries(tst_mime_apps_service PRIVATE
    Qt6::Core
    Qt6::Test
    astrea_explorer_native_operations
)

add_test(NAME mime_apps_service COMMAND tst_mime_apps_service)

qt_add_executable(tst_portal_controller
    tests/tst_portal_controller.cpp
)

target_include_directories(tst_portal_controller PRIVATE
    src
)

target_link_libraries(tst_portal_controller PRIVATE
    Qt6::Core
    Qt6::Test
    astrea_explorer_native_operations
)

add_test(NAME portal_controller COMMAND tst_portal_controller)

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
    astrea_explorer_native_operations
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

qt_add_executable(tst_recent_store
    tests/tst_recent_store.cpp
)

target_include_directories(tst_recent_store PRIVATE
    src
)

target_link_libraries(tst_recent_store PRIVATE
    Qt6::Core
    Qt6::Test
    astrea_explorer_native_recent
)

add_test(NAME recent_store COMMAND tst_recent_store)

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
    astrea_explorer_native_operations
)

add_test(NAME app_state_facade COMMAND tst_app_state_facade)

qt_add_executable(tst_shadow_parity
    tests/parity/fixture_tree.h
    tests/parity/fixture_tree.cpp
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
    ASTREA_EXPLORER_PARITY_BACKEND="${ASTREA_EXPLORER_BACKEND_ARTIFACT}"
)

target_link_libraries(tst_shadow_parity PRIVATE
    Qt6::Core
    Qt6::Test
    astrea_explorer_native_navigation
)

add_test(NAME shadow_parity COMMAND tst_shadow_parity)

add_subdirectory(tests)

# Production installation is rooted at ${CMAKE_INSTALL_PREFIX}/share/Astrea so
# the runtime resolver remains independent of the source checkout. QML is
# installed as source presentation; required Rust and launch artifacts are
# built or copied as part of this production target.
install(TARGETS astrea-explorer
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

set(ASTREA_INSTALL_RUNTIME_ROOT "${CMAKE_INSTALL_DATADIR}/Astrea")
install(DIRECTORY "${ASTREA_EXPLORER_RUNTIME_ROOT}/Apps/Explorer"
    DESTINATION "${ASTREA_INSTALL_RUNTIME_ROOT}/Apps"
    FILES_MATCHING
        PATTERN "*.qml"
        PATTERN "*.js"
        PATTERN "qmldir"
        PATTERN "*.json"
        PATTERN "*.svg"
        PATTERN "*.png"
        PATTERN "*.jpg"
        PATTERN "*.jpeg"
        PATTERN "native" EXCLUDE
        PATTERN "tests" EXCLUDE
        PATTERN "__pycache__" EXCLUDE
        PATTERN "*.pyc" EXCLUDE
        PATTERN "QuickshellComponents" EXCLUDE
        PATTERN "build*" EXCLUDE)
install(DIRECTORY "${ASTREA_EXPLORER_RUNTIME_ROOT}/Core/components"
    DESTINATION "${ASTREA_INSTALL_RUNTIME_ROOT}/Core"
    FILES_MATCHING PATTERN "*.qml" PATTERN "*.js" PATTERN "qmldir"
        PATTERN "*.json" PATTERN "*.svg" PATTERN "*.png")
install(DIRECTORY "${ASTREA_EXPLORER_RUNTIME_ROOT}/Features/Files"
    DESTINATION "${ASTREA_INSTALL_RUNTIME_ROOT}/Features"
    FILES_MATCHING PATTERN "*.qml" PATTERN "*.js" PATTERN "qmldir")
install(DIRECTORY "${ASTREA_EXPLORER_RUNTIME_ROOT}/System/i18n"
    DESTINATION "${ASTREA_INSTALL_RUNTIME_ROOT}/System"
    FILES_MATCHING PATTERN "*.qml" PATTERN "*.js" PATTERN "qmldir"
        PATTERN "*.json")
install(PROGRAMS "${ASTREA_EXPLORER_BACKEND_ARTIFACT}"
    DESTINATION "${ASTREA_INSTALL_RUNTIME_ROOT}/Core/bridge/apps"
    RENAME explorer_backend)
install(PROGRAMS "${ASTREA_EXPLORER_PORTAL_ARTIFACT}"
    DESTINATION "${ASTREA_INSTALL_RUNTIME_ROOT}/bin"
    RENAME astrea-filechooser-portal)
install(PROGRAMS "${ASTREA_EXPLORER_LAUNCH_PROGRAM}"
    DESTINATION "${ASTREA_INSTALL_RUNTIME_ROOT}/bin"
    RENAME astrea-launch)
install(PROGRAMS
    "${CMAKE_CURRENT_SOURCE_DIR}/../../../System/services/astrea-services.sh"
    DESTINATION "${ASTREA_INSTALL_RUNTIME_ROOT}/System/services")
