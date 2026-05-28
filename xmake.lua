add_rules("mode.debug", "mode.release")

set_config("plat", "mingw")
set_config("mingw", "C:/msys64/ucrt64")
set_config("qt",    "C:/msys64/ucrt64")
set_languages("c++17")

set_warnings("all")
add_defines("WIN32_LEAN_AND_MEAN", "NOMINMAX", "UNICODE")

add_requires("spdlog",        { configs = { header_only = true } })
add_requires("nlohmann_json", { configs = { header_only = true } })
add_requires("entt")

target("UGVControlStation")
    add_rules("qt.widgetapp")
    add_files(
        "resources/app.qrc",
        "app.rc",
        "src/main.cpp",
        "src/ui/MainWindow.cpp",
        "src/ui/panels/ConnectionPanel.cpp",
        "src/ui/panels/ControlPanel.cpp",
        "src/ui/panels/TelemetryPanel.cpp",
        "src/ui/panels/LogsPanel.cpp",
        "src/ui/panels/LegendPanel.cpp",
        "src/services/SafetyService.cpp",
        "src/services/ControlService.cpp",
        "src/io/SerialPort.cpp",
        "src/io/SerialWorker.cpp",
        "src/crsf/CrsfPacket.cpp",
        "src/input/KeyboardInput.cpp",
        "src/input/XInputGamepad.cpp",
        "src/input/InputManager.cpp",
        "src/config/AppConfig.cpp"
    )
    add_includedirs("src")
    add_packages("spdlog", "nlohmann_json", "entt")
    add_syslinks("kernel32", "user32", "winmm", "setupapi", "xinput")

    if is_mode("release") then
        set_optimize("faster")
        add_defines("NDEBUG")
    end
    if is_mode("debug") then
        set_optimize("none")
        set_symbols("debug")
    end
