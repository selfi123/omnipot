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

# Realtime subtitle needs whisper.cpp; without it Omnipot still builds, minus that feature
option(OMNIPOT_REALTIME_SUBTITLE "Build the Realtime subtitle feature (offline speech-to-text)" ON)
set(OMNIPOT_HAVE_WHISPER 0)
if(OMNIPOT_REALTIME_SUBTITLE)
    # Prefer lightweight bundled whisper.cpp via FetchContent so users don't need heavy distro packages
    option(OMNIPOT_USE_SYSTEM_WHISPER "Use system-installed whisper.cpp instead of lightweight bundled build" OFF)

    if(OMNIPOT_USE_SYSTEM_WHISPER)
        find_package(whisper CONFIG QUIET)
        if(TARGET whisper)
            set(OMNIPOT_WHISPER_TARGET whisper)
        else()
            pkg_check_modules(WHISPER QUIET IMPORTED_TARGET whisper)
            if(WHISPER_FOUND)
                set(OMNIPOT_WHISPER_TARGET PkgConfig::WHISPER)
            else()
                find_path(WHISPER_INCLUDE_DIR NAMES whisper.h PATH_SUFFIXES whisper)
                find_library(WHISPER_LIB NAMES whisper whisper-cpp)
                if(WHISPER_INCLUDE_DIR AND WHISPER_LIB)
                    add_library(whisper_imported UNKNOWN IMPORTED)
                    set_target_properties(whisper_imported PROPERTIES
                        IMPORTED_LOCATION "${WHISPER_LIB}"
                        INTERFACE_INCLUDE_DIRECTORIES "${WHISPER_INCLUDE_DIR}"
                    )
                    set(OMNIPOT_WHISPER_TARGET whisper_imported)
                endif()
            endif()
        endif()
        if(OMNIPOT_WHISPER_TARGET)
            find_library(GGML_LIB NAMES ggml)
            if(GGML_LIB)
                list(APPEND OMNIPOT_WHISPER_TARGET ${GGML_LIB})
            endif()
        endif()
    endif()

    if(NOT OMNIPOT_WHISPER_TARGET)
        message(STATUS "Realtime subtitle: fetching lightweight bundled whisper.cpp (no external GPU dependencies)...")
        include(FetchContent)
        set(WHISPER_BUILD_TESTS OFF CACHE BOOL "" FORCE)
        set(WHISPER_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
        set(WHISPER_BUILD_SERVER OFF CACHE BOOL "" FORCE)
        set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)

        FetchContent_Declare(
            whisper
            GIT_REPOSITORY https://github.com/ggerganov/whisper.cpp.git
            GIT_TAG        v1.7.4
            GIT_SHALLOW    TRUE
        )
        FetchContent_MakeAvailable(whisper)
        set(OMNIPOT_WHISPER_TARGET whisper)
    endif()

    if(OMNIPOT_WHISPER_TARGET)
        set(OMNIPOT_HAVE_WHISPER 1)
        message(STATUS "Realtime subtitle: enabled (whisper.cpp ready)")
    endif()
endif()

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
    include/subtitle/subtitle_timeline.h
    include/subtitle/whisper_model.h
    include/subtitle/speech_activity.h
    include/subtitle/realtime_subtitle.h
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
    src/subtitle/subtitle_timeline.cpp
    src/subtitle/whisper_model.cpp
    src/subtitle/speech_activity.cpp
    src/subtitle/realtime_subtitle.cpp
    ${COMMON_HEADERS}
)

if(OMNIPOT_HAVE_WHISPER)
    list(APPEND COMMON_SOURCES
        src/subtitle/realtime_subtitle_worker.cpp
        include/subtitle/realtime_subtitle_worker.h
    )
endif()

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
target_compile_definitions(omnipot PRIVATE OMNIPOT_HAVE_WHISPER=${OMNIPOT_HAVE_WHISPER})
if(OMNIPOT_HAVE_WHISPER)
    target_link_libraries(omnipot PRIVATE ${OMNIPOT_WHISPER_TARGET})
endif()

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

add_executable(test_realtime_subtitle
    tests/test_realtime_subtitle.cpp
    src/subtitle/subtitle_timeline.cpp
    src/subtitle/whisper_model.cpp
    src/subtitle/speech_activity.cpp
    include/subtitle/subtitle_timeline.h
    include/subtitle/whisper_model.h
    include/subtitle/speech_activity.h
)

target_link_libraries(test_realtime_subtitle PRIVATE
    Qt6::Core
    Qt6::Test
)

add_test(NAME test_realtime_subtitle COMMAND test_realtime_subtitle)

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
set(CPACK_PACKAGE_CONTACT "https://github.com/selfi123/omnipot")

set(CPACK_GENERATOR "TGZ;RPM")
set(CPACK_RPM_PACKAGE_LICENSE "GPLv3")
set(CPACK_RPM_PACKAGE_GROUP "Applications/Multimedia")
set(CPACK_RPM_PACKAGE_REQUIRES "qt6-qtbase >= 6.5, mpv-libs >= 0.35, zlib")
# ffmpeg and whisper.cpp are optional at runtime (Audio Extraction, Realtime subtitle)
set(CPACK_RPM_PACKAGE_SUGGESTS "ffmpeg, whisper-cpp")

include(CPack)
