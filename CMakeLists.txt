cmake_minimum_required(VERSION 3.21)

project(Orbit VERSION 0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Qt6 6.11 REQUIRED COMPONENTS Core Gui Qml Quick QuickControls2 Test)
qt_standard_project_setup(REQUIRES 6.11)

include(GNUInstallDirs)
include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/OrbitRust.cmake)

enable_testing()
add_subdirectory(apps/explorer)
