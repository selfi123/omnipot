cmake_minimum_required(VERSION 3.25)
project(omnipot
    VERSION 1.0.0
    DESCRIPTION "Native Qt 6 + libmpv media player for Linux"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Compiler flags
add_compile_options(-Wall -Wextra -Wpedantic)

# Qt 6 Components
find_package(Qt6 6.5 REQUIRED COMPONENTS Core Gui Widgets Network OpenGL OpenGLWidgets Test)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# Find libmpv using PkgConfig or fallback
find_package(PkgConfig REQUIRED)
pkg_check_modules(MPV REQUIRED IMPORTED_TARGET mpv)

find_package(ZLIB REQUIRED)

include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# Common headers for AUTOMOC
set(COMMON_HEADERS
    include/app/single_instance.h
    include/playback/playback_engine.h
    include/playback/mpv_engine.h
    include/video/video_widget.h
    include/ui/time_bar.h
    include/ui/main_window.h
    include/playlist/playlist_item.h
    include/playlist/dpl_parser.h
    include/playlist/m3u_parser.h
    include/playlist/playlist_manager.h
    include/config/settings_manager.h
    include/ui/playlist_drawer.h
    include/ui/control_box.h
    include/ui/info_overlay.h
    include/video/shader_manager.h
    include/video/frame_capture.h
    include/ui/consecutive_capture_dialog.h
    include/ui/audio_extract_dialog.h
    include/ui/subtitle_search_dialog.h
    include/ui/open_url_dialog.h
    include/ui/preferences_dialog.h
    include/ui/title_bar.h
    include/ui/omni_icons.h
)

# Common core sources
set(COMMON_SOURCES
    src/app/single_instance.cpp
    src/playback/playback_engine.cpp
    src/playback/mpv_engine.cpp
    src/video/video_widget.cpp
    src/ui/time_bar.cpp
    src/ui/main_window.cpp
    src/ui/title_bar.cpp
    src/ui/omni_icons.cpp
    src/playlist/dpl_parser.cpp
    src/playlist/m3u_parser.cpp
    src/playlist/playlist_manager.cpp
    src/config/settings_manager.cpp
    src/ui/playlist_drawer.cpp
    src/ui/control_box.cpp
    src/ui/info_overlay.cpp
    src/video/shader_manager.cpp
    src/video/frame_capture.cpp
    src/ui/consecutive_capture_dialog.cpp
    src/ui/audio_extract_dialog.cpp
    src/ui/subtitle_search_dialog.cpp
    src/ui/open_url_dialog.cpp
    src/ui/preferences_dialog.cpp
    ${COMMON_HEADERS}
)

# Main Application Executable
add_executable(omnipot
    src/main.cpp
    ${COMMON_SOURCES}
)

target_link_libraries(omnipot PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Network
    Qt6::OpenGL
    Qt6::OpenGLWidgets
    PkgConfig::MPV
    ZLIB::ZLIB
)

# Testing
enable_testing()

add_executable(test_playback_engine
    tests/test_playback_engine.cpp
    src/playback/playback_engine.cpp
    src/playback/mpv_engine.cpp
    src/config/settings_manager.cpp
    src/ui/time_bar.cpp
    src/ui/omni_icons.cpp
    include/playback/playback_engine.h
    include/playback/mpv_engine.h
    include/config/settings_manager.h
    include/ui/time_bar.h
    include/ui/omni_icons.h
)

target_link_libraries(test_playback_engine PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Test
    PkgConfig::MPV
)

add_test(NAME test_playback_engine COMMAND test_playback_engine)

add_executable(test_playlist
    tests/test_playlist.cpp
    src/playlist/dpl_parser.cpp
    src/playlist/m3u_parser.cpp
    src/playlist/playlist_manager.cpp
    src/config/settings_manager.cpp
    include/playlist/playlist_item.h
    include/playlist/dpl_parser.h
    include/playlist/m3u_parser.h
    include/playlist/playlist_manager.h
    include/config/settings_manager.h
)

target_link_libraries(test_playlist PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Test
)

add_test(NAME test_playlist COMMAND test_playlist)

add_executable(test_shaders
    tests/test_shaders.cpp
    src/video/shader_manager.cpp
    src/config/settings_manager.cpp
    include/video/shader_manager.h
    include/config/settings_manager.h
)

target_link_libraries(test_shaders PRIVATE
    Qt6::Core
    Qt6::Test
    PkgConfig::MPV
)

add_test(NAME test_shaders COMMAND test_shaders)

add_executable(test_frame_capture
    tests/test_frame_capture.cpp
    src/video/frame_capture.cpp
    include/video/frame_capture.h
)

target_link_libraries(test_frame_capture PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Test
    PkgConfig::MPV
)

add_test(NAME test_frame_capture COMMAND test_frame_capture)

# Installation
include(GNUInstallDirs)
install(TARGETS omnipot DESTINATION ${CMAKE_INSTALL_BINDIR})
install(FILES data/omnipot.desktop DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/applications)
install(DIRECTORY data/icons/ DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/icons/hicolor)

# Packaging with CPack
set(CPACK_PACKAGE_NAME "omnipot")
set(CPACK_PACKAGE_VENDOR "Omnipot Contributors")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Native media player for Linux built on Qt 6 and libmpv")
set(CPACK_PACKAGE_VERSION "1.0.0")
set(CPACK_PACKAGE_VERSION_MAJOR "1")
set(CPACK_PACKAGE_VERSION_MINOR "0")
set(CPACK_PACKAGE_VERSION_PATCH "0")
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE")
set(CPACK_RESOURCE_FILE_README "${CMAKE_CURRENT_SOURCE_DIR}/README.md")
set(CPACK_PACKAGE_CONTACT "https://github.com/selfi123/potplayer-linux")

set(CPACK_GENERATOR "TGZ;RPM")
set(CPACK_RPM_PACKAGE_LICENSE "GPLv3")
set(CPACK_RPM_PACKAGE_GROUP "Applications/Multimedia")
set(CPACK_RPM_PACKAGE_REQUIRES "qt6-qtbase >= 6.5, mpv-libs >= 0.35, zlib")

include(CPack)
