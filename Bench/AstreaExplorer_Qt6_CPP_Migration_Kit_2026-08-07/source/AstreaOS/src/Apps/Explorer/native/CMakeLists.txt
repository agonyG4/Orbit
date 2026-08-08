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
)

target_include_directories(astrea_explorer_native_navigation PUBLIC
    src
)

target_link_libraries(astrea_explorer_native_navigation PUBLIC
    Qt6::Core
    astrea_explorer_native_backend
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

add_subdirectory(tests)
