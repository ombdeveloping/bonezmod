cmake_minimum_required(VERSION 3.24)
project(bonezmod LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT WIN32)
    message(FATAL_ERROR "bonezmod is Windows-only.")
endif()

add_compile_definitions(
    UNICODE _UNICODE
    NOMINMAX
    WIN32_LEAN_AND_MEAN
    _CRT_SECURE_NO_WARNINGS
)

add_executable(bonezmod WIN32
    src/main.cpp
    src/eac_guard.cpp
    src/process_watch.cpp
    src/overlay/overlay.cpp
    src/overlay/rank_badge.cpp
    src/api/tracker_api.cpp
    src/api/log_watcher.cpp
    src/plugins/mmr_reveal.cpp
)

target_include_directories(bonezmod PRIVATE src)

target_link_libraries(bonezmod PRIVATE
    d3d11 dxgi dcomp d2d1 dwrite
    user32 gdi32 shell32 ole32 uuid
    winhttp windowscodecs
    psapi advapi32
)

set_target_properties(bonezmod PROPERTIES VS_DPI_AWARE "PerMonitor")
