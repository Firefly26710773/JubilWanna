cmake_minimum_required(VERSION 3.20)
project(JubiIWanna VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(JubiIWanna WIN32
    src/main.cpp
    src/Game.cpp
    src/Game.h
    src/Level.cpp
    src/Level.h
    src/Player.cpp
    src/Player.h
    src/Renderer.cpp
    src/Renderer.h
    src/Audio.cpp
    src/Audio.h
    src/Common.h
    app.manifest
)

target_include_directories(JubiIWanna PRIVATE src)
target_compile_options(JubiIWanna PRIVATE /utf-8 /W3 /EHsc)
target_link_libraries(JubiIWanna PRIVATE user32 gdi32 winmm msimg32 shell32)

# Standalone executable without the VC++ redistributable.
set_property(TARGET JubiIWanna PROPERTY
    MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

if (MSVC)
    target_compile_definitions(JubiIWanna PRIVATE UNICODE _UNICODE NOMINMAX)
    target_link_options(JubiIWanna PRIVATE
        "/MANIFEST:EMBED"
        "/MANIFESTINPUT:${CMAKE_CURRENT_SOURCE_DIR}/app.manifest")
endif()
