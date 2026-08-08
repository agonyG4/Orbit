cmake_minimum_required(VERSION 3.21)

project(AstreaExplorerNative LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Qt6 6.11 REQUIRED COMPONENTS Core Gui Qml Quick QuickControls2 Test)
qt_standard_project_setup(REQUIRES 6.11)

enable_testing()

set(ASTREA_EXPLORER_BIN "${CMAKE_BINARY_DIR}/astrea-explorer")

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
)

qt_add_qml_module(astrea-explorer
    URI Astrea.Explorer.Native
    VERSION 1.0
    QML_FILES
        qml/NativeBootstrap.qml
)

add_subdirectory(tests)
