cmake_minimum_required(VERSION 3.21)
project(UGVControlStation LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_compile_definitions(WIN32_LEAN_AND_MEAN NOMINMAX UNICODE)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets)

qt_standard_project_setup()

include(FetchContent)

FetchContent_Declare(
    spdlog
    GIT_REPOSITORY https://github.com/gabime/spdlog.git
    GIT_TAG        v1.17.0
)
set(SPDLOG_BUILD_EXAMPLE OFF CACHE BOOL "" FORCE)
set(SPDLOG_BUILD_TESTS OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(spdlog)

FetchContent_Declare(
    nlohmann_json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG        v3.12.0
)
FetchContent_MakeAvailable(nlohmann_json)

FetchContent_Declare(
    entt
    GIT_REPOSITORY https://github.com/skypjack/entt.git
    GIT_TAG        v3.16.0
)
FetchContent_MakeAvailable(entt)

enable_language(RC)
set(CMAKE_RC_COMPILER_INIT windres)
set(CMAKE_RC_COMPILE_OBJECT "<CMAKE_RC_COMPILER> -O coff <DEFINES> -I<CMAKE_CURRENT_SOURCE_DIR> -i <SOURCE> -o <OBJECT>")

qt_add_executable(UGVControlStation WIN32
    resources/app.qrc
    app.rc
    src/main.cpp
    src/ui/MainWindow.cpp
    src/ui/SettingsDialog.cpp
    src/ui/SettingsDialog.h
    src/ui/CompassBar.cpp
    src/ui/CompassBar.h
    src/ui/widgets/ToggleSwitch.cpp
    src/ui/panels/ConnectionPanel.cpp
    src/ui/panels/ControlPanel.cpp
    src/ui/panels/TelemetryPanel.cpp
    src/ui/panels/LogsPanel.cpp
    src/ui/panels/LegendPanel.cpp
    src/ui/panels/VideoPanel.cpp
    src/ui/panels/VideoPanel.h
    src/ui/panels/WheelPanel.cpp
    src/ui/panels/WheelPanel.h
    src/services/SafetyService.cpp
    src/services/ControlService.cpp
    src/io/SerialPort.cpp
    src/io/SerialWorker.cpp
    src/crsf/CrsfPacket.cpp
    src/input/KeyboardInput.cpp
    src/input/XInputGamepad.cpp
    src/input/InputManager.cpp
    src/config/AppConfig.cpp
)

target_include_directories(UGVControlStation PRIVATE src)

target_link_libraries(UGVControlStation PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    spdlog::spdlog
    nlohmann_json::nlohmann_json
    EnTT::EnTT
    kernel32
    user32
    winmm
    setupapi
    xinput
)

if(CMAKE_BUILD_TYPE STREQUAL "Release")
    target_compile_definitions(UGVControlStation PRIVATE NDEBUG)
endif()
