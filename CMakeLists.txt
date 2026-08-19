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

# --- Optional deps ---------------------------------------------------------
option(BONEZ_WITH_OPENCV "Enable OpenCV-based vision detectors" ON)
option(BONEZ_WITH_VIGEM  "Enable ViGEm virtual pad output"      ON)
option(BONEZ_WITH_IMGUI  "Enable ImGui control panel"           ON)

if(BONEZ_WITH_OPENCV)
    find_package(OpenCV QUIET)
    if(NOT OpenCV_FOUND)
        message(WARNING "OpenCV not found; vision will build stubs.")
        set(BONEZ_WITH_OPENCV OFF)
    endif()
endif()

# ViGEmClient: expected under third_party/ViGEmClient (submodule / vendored)
if(BONEZ_WITH_VIGEM AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/third_party/ViGEmClient/CMakeLists.txt")
    add_subdirectory(third_party/ViGEmClient)
else()
    message(WARNING "ViGEmClient not vendored; virtual pad will be stubbed.")
    set(BONEZ_WITH_VIGEM OFF)
endif()

if(BONEZ_WITH_IMGUI AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/third_party/imgui/imgui.h")
    file(GLOB IMGUI_SRC
        third_party/imgui/*.cpp
        third_party/imgui/backends/imgui_impl_win32.cpp
        third_party/imgui/backends/imgui_impl_dx11.cpp)
    add_library(imgui STATIC ${IMGUI_SRC})
    target_include_directories(imgui PUBLIC
        third_party/imgui
        third_party/imgui/backends)
else()
    message(WARNING "ImGui not vendored; control panel will be stubbed.")
    set(BONEZ_WITH_IMGUI OFF)
endif()

# --- App -------------------------------------------------------------------
add_executable(bonezmod WIN32
    src/main.cpp
    src/eac_guard.cpp
    src/process_watch.cpp
    src/capture/dxgi_capture.cpp
    src/vision/detector.cpp
    src/overlay/overlay.cpp
    src/overlay/rank_badge.cpp
    src/input/vigem_pad.cpp
    src/input/macro.cpp
    src/replay/replay_parser.cpp
    src/api/tracker_api.cpp
    src/api/log_watcher.cpp
    src/plugins/mmr_reveal.cpp
    src/plugins/freeplay_reset.cpp
    src/plugins/training_pack.cpp
    src/plugins/shot_mirror.cpp
    src/plugins/auto_queue.cpp
    src/plugins/workshop_manager.cpp
    src/plugins/camera_preset.cpp
    src/plugins/ball_trail.cpp
    src/plugins/soundboard.cpp
    src/plugins/obs_replay.cpp
    src/plugins/chat.cpp
    src/ui/controlpanel.cpp
)

target_include_directories(bonezmod PRIVATE src)

target_link_libraries(bonezmod PRIVATE
    d3d11 dxgi dcomp d2d1 dwrite dwmapi
    user32 gdi32 shell32 ole32 uuid
    winhttp winmm bcrypt windowscodecs
    psapi advapi32
)

if(BONEZ_WITH_OPENCV)
    target_compile_definitions(bonezmod PRIVATE BONEZ_HAS_OPENCV=1)
    target_link_libraries(bonezmod PRIVATE ${OpenCV_LIBS})
    target_include_directories(bonezmod PRIVATE ${OpenCV_INCLUDE_DIRS})
endif()

if(BONEZ_WITH_VIGEM)
    target_compile_definitions(bonezmod PRIVATE BONEZ_HAS_VIGEM=1)
    target_link_libraries(bonezmod PRIVATE ViGEmClient)
endif()

if(BONEZ_WITH_IMGUI)
    target_compile_definitions(bonezmod PRIVATE BONEZ_HAS_IMGUI=1)
    target_link_libraries(bonezmod PRIVATE imgui)
endif()

# App manifest: requestedExecutionLevel=asInvoker, DPI aware
set_target_properties(bonezmod PROPERTIES
    VS_DPI_AWARE "PerMonitor")
