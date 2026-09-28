cmake_minimum_required(VERSION 3.21)
project(cqptur LANGUAGES CXX)

add_executable(cqptur WIN32
    src/main.cpp
    src/app.cpp
    src/capture.cpp
    src/overlay.cpp
    src/output.cpp
    src/settings.cpp
    src/startup.cpp
    src/tray.cpp
    src/cqptur.rc     # icon and version info
    src/app.manifest  # per-monitor DPI awareness: the whole point of this tool
)

target_compile_features(cqptur PRIVATE cxx_std_20)
target_compile_definitions(cqptur PRIVATE UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX)
target_compile_options(cqptur PRIVATE /W4 /permissive- /utf-8)
target_link_libraries(cqptur PRIVATE shcore windowscodecs)

# Static C++ runtime: the .exe runs on any Windows 10/11 PC without the VC++ redistributable.
set_property(TARGET cqptur PROPERTY
    MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
