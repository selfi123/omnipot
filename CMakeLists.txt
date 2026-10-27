cmake_minimum_required(VERSION 3.25)
project(potplayer-linux
    VERSION 1.0.0
    DESCRIPTION "Native PotPlayer Media Player Clone for Fedora Linux"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Compiler flags
add_compile_options(-Wall -Wextra -Wpedantic)

# Qt 6 Components
find_package(Qt6 6.5 REQUIRED COMPONENTS Core Gui Widgets Test)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# Find libmpv using PkgConfig or fallback
find_package(PkgConfig REQUIRED)
pkg_check_modules(MPV REQUIRED IMPORTED_TARGET mpv)

include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# Common core sources
set(COMMON_SOURCES
    src/playback/mpv_engine.cpp
    src/video/video_widget.cpp
    src/ui/time_bar.cpp
    src/ui/main_window.cpp
)

# Main Application Executable
add_executable(potplayer-linux
    src/main.cpp
    ${COMMON_SOURCES}
)

target_link_libraries(potplayer-linux PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    PkgConfig::MPV
)

# Testing
enable_testing()

add_executable(test_playback_engine
    tests/test_playback_engine.cpp
    src/playback/mpv_engine.cpp
    src/ui/time_bar.cpp
)

target_link_libraries(test_playback_engine PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Test
    PkgConfig::MPV
)

add_test(NAME test_playback_engine COMMAND test_playback_engine)
