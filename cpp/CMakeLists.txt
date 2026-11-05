cmake_minimum_required(VERSION 3.20)
project(IraqTheaterCommand VERSION 0.7.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Default to Release build if not specified for maximum 2D/3D simulation & instancing performance
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

option(ITC_FORCE_FETCH_RAYLIB "Always fetch and statically compile Raylib from source" OFF)
option(ITC_WIN32_GUI "Build Windows executable as a GUI subsystem app without console window" ON)

# -----------------------------------------------------------------------------
# Dependencies: Raylib (Static-first for 100% standalone Mac & Windows installers)
# -----------------------------------------------------------------------------
if(NOT ITC_FORCE_FETCH_RAYLIB)
    find_package(raylib QUIET)
endif()

if(raylib_FOUND)
    message(STATUS "[ITC] Found system raylib (${raylib_VERSION})")
    # Prefer static libraylib.a on macOS so distributed .app/.dmg/.pkg has zero Homebrew dylib dependencies
    find_library(RAYLIB_STATIC_LIB NAMES libraylib.a PATHS /opt/homebrew/opt/raylib/lib /usr/local/opt/raylib/lib NO_DEFAULT_PATH)
    if(APPLE AND RAYLIB_STATIC_LIB)
        message(STATUS "[ITC] Using static Raylib archive for standalone macOS binary: ${RAYLIB_STATIC_LIB}")
        find_path(RAYLIB_INCLUDE_DIR NAMES raylib.h PATHS /opt/homebrew/opt/raylib/include /usr/local/opt/raylib/include NO_DEFAULT_PATH)
        add_library(itc_raylib STATIC IMPORTED)
        set_target_properties(itc_raylib PROPERTIES
            IMPORTED_LOCATION "${RAYLIB_STATIC_LIB}"
            INTERFACE_INCLUDE_DIRECTORIES "${RAYLIB_INCLUDE_DIR}"
        )
        set(ITC_RAYLIB_TARGET itc_raylib)
    else()
        set(ITC_RAYLIB_TARGET raylib)
    endif()
else()
    message(STATUS "[ITC] Fetching and building static raylib 5.5 via FetchContent...")
    include(FetchContent)
    set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(BUILD_GAMES OFF CACHE BOOL "" FORCE)
    set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
    FetchContent_Declare(
        raylib
        GIT_REPOSITORY https://github.com/raysan5/raylib.git
        GIT_TAG 5.5
        GIT_SHALLOW TRUE
    )
    FetchContent_MakeAvailable(raylib)
    set(ITC_RAYLIB_TARGET raylib)
endif()

# -----------------------------------------------------------------------------
# Optional FMOD Studio Engine Support (Drop FMOD SDK into cpp/third_party/fmod)
# -----------------------------------------------------------------------------
option(ITC_ENABLE_FMOD "Enable FMOD Studio low-level/bank audio backend if SDK is present" OFF)
set(FMOD_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/third_party/fmod" CACHE PATH "Path to FMOD Studio API SDK")

# -----------------------------------------------------------------------------
# Executable Target
# -----------------------------------------------------------------------------
set(ITC_SOURCES
    src/main.cpp
    src/asset_loader.cpp
    src/renderer_2d.cpp
    src/renderer_3d.cpp
    src/ui_panels.cpp
    src/audio_engine.cpp
)

set(ITC_HEADERS
    src/sim_types.hpp
    src/asset_loader.hpp
    src/renderer_2d.hpp
    src/renderer_3d.hpp
    src/ui_panels.hpp
    src/audio_engine.hpp
)

add_executable(IraqTheaterCommand ${ITC_SOURCES} ${ITC_HEADERS})

target_include_directories(IraqTheaterCommand PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
target_link_libraries(IraqTheaterCommand PRIVATE ${ITC_RAYLIB_TARGET})

# Pass default asset directory path for development runs while also checking relative ./assets and .app Resources
target_compile_definitions(IraqTheaterCommand PRIVATE
    ITC_SOURCE_ASSETS_DIR="${CMAKE_CURRENT_SOURCE_DIR}/assets"
)

if(ITC_ENABLE_FMOD AND EXISTS "${FMOD_ROOT}/api/core/inc/fmod.hpp")
    message(STATUS "[ITC] FMOD SDK detected at ${FMOD_ROOT} — enabling FMOD backend")
    target_compile_definitions(IraqTheaterCommand PRIVATE ITC_HAS_FMOD=1)
    target_include_directories(IraqTheaterCommand PRIVATE
        "${FMOD_ROOT}/api/core/inc"
        "${FMOD_ROOT}/api/studio/inc"
    )
endif()

# -----------------------------------------------------------------------------
# Platform-specific Compiler & Linker Configuration (macOS & Windows)
# -----------------------------------------------------------------------------
if(APPLE)
    target_link_libraries(IraqTheaterCommand PRIVATE
        "-framework IOKit"
        "-framework Cocoa"
        "-framework OpenGL"
        "-framework CoreAudio"
        "-framework CoreVideo"
        "-framework AudioToolbox"
        "-framework QuartzCore"
        "-framework Metal"
    )
    target_compile_options(IraqTheaterCommand PRIVATE -Wall -Wextra -Wno-missing-field-initializers -O3)
elseif(WIN32)
    target_link_libraries(IraqTheaterCommand PRIVATE winmm gdi32 opengl32 shell32 user32)
    if(MSVC)
        target_compile_options(IraqTheaterCommand PRIVATE /W3 /O2 /permissive- /utf-8)
        target_compile_definitions(IraqTheaterCommand PRIVATE _CRT_SECURE_NO_WARNINGS NOMINMAX WIN32_LEAN_AND_MEAN)
        if(ITC_WIN32_GUI)
            target_link_options(IraqTheaterCommand PRIVATE "/SUBSYSTEM:WINDOWS" "/ENTRY:mainCRTStartup")
        endif()
    else()
        target_compile_options(IraqTheaterCommand PRIVATE -Wall -Wextra -O3)
        if(ITC_WIN32_GUI)
            target_link_options(IraqTheaterCommand PRIVATE "-mwindows")
        endif()
    endif()
else()
    target_compile_options(IraqTheaterCommand PRIVATE -Wall -Wextra -O3)
    target_link_libraries(IraqTheaterCommand PRIVATE m pthread dl)
endif()

# -----------------------------------------------------------------------------
# Installation & CPack Rules
# -----------------------------------------------------------------------------
install(TARGETS IraqTheaterCommand RUNTIME DESTINATION .)
install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/assets" DESTINATION .)

set(CPACK_PACKAGE_NAME "IraqTheaterCommand")
set(CPACK_PACKAGE_VENDOR "OIFsim")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Iraq Theater Command — C++20 Tactical & Strategic Simulation")
set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
set(CPACK_PACKAGE_INSTALL_DIRECTORY "Iraq Theater Command")
include(CPack)
