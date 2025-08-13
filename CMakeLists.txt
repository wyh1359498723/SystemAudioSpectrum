cmake_minimum_required(VERSION 3.16)
project(SystemAudioSpectrum LANGUAGES C CXX)

include(qt.cmake)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Core Widgets Gui)
find_package(Qt${QT_VERSION_MAJOR} COMPONENTS Core Widgets Gui REQUIRED)
qt_standard_project_setup()

set(PROJECT_SOURCES
    main.cpp
    SystemAudioSpectrum.ui
    SystemAudioSpectrum.h
    SystemAudioSpectrum.cpp
    src/RingBuffer.h
    src/RingBuffer.cpp
    src/AudioCapture.h
    src/AudioCapture.cpp
    src/FFTProcessor.h
    src/FFTProcessor.cpp
    src/Renderer.h
    src/Renderer.cpp
)

# Disable kissfft extras to avoid pkg-config dependency and extra targets
set(KISSFFT_PKGCONFIG OFF CACHE BOOL "Build pkg-config files" FORCE)
set(KISSFFT_TEST OFF CACHE BOOL "Build kissfft tests" FORCE)
set(KISSFFT_TOOLS OFF CACHE BOOL "Build kissfft command-line tools" FORCE)

add_subdirectory(thirdparty/kissfft)

add_executable(${PROJECT_NAME} ${PROJECT_SOURCES})

set_target_properties(${PROJECT_NAME}
    PROPERTIES
        WIN32_EXECUTABLE TRUE
)

target_include_directories(${PROJECT_NAME}
    PRIVATE
        src
        thirdparty/kissfft
)

target_link_libraries(${PROJECT_NAME}
    PUBLIC
        Qt::Core
        Qt::Gui
        Qt::Widgets
        kissfft
        ole32
        mmdevapi
        avrt
)