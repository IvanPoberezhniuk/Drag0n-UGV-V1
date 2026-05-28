#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include <QIcon>
#include <QApplication>
#include <QMessageBox>
#include <QSettings>
#include <QFont>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include "core/AppState.h"
#include "core/ControlState.h"
#include "core/TelemetryState.h"
#include "core/SafetyState.h"
#include "core/ConnectionState.h"
#include "core/LogBuffer.h"
#include "config/AppConfig.h"
#include "io/SerialWorker.h"
#include "input/InputManager.h"
#include "input/KeyboardInput.h"
#include "input/XInputGamepad.h"
#include "ui/MainWindow.h"
#include "ui/Theme.h"
#include "ui/SettingsKeys.h"
#include <filesystem>
#include <memory>
#include <string>
#include <mutex>

static std::string findConfig(int argc, char** argv) {
    for (int i = 1; i < argc - 1; ++i)
        if (std::string(argv[i]) == "--config")
            return argv[i + 1];

    std::filesystem::path exeDir = std::filesystem::path(argv[0]).parent_path();
    auto candidate = exeDir / "config.json";
    if (std::filesystem::exists(candidate))
        return candidate.string();

    if (std::filesystem::exists("config.json"))
        return "config.json";

    return {};
}

int main(int argc, char** argv) {
    bool verbose = false;
    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        if (a == "--verbose" || a == "-v") verbose = true;
        if (a == "--help"    || a == "-h") {
            printf("Usage: UGVControlStation [--config <path>] [--verbose]\n");
            return 0;
        }
    }

    QApplication app(argc, argv);
    app.setWindowIcon(QIcon(":/icon.png"));

    // Single-instance guard via Win32 named mutex
    HANDLE instanceMutex = CreateMutexW(nullptr, TRUE, L"UGVControlStation-single-instance");
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        CloseHandle(instanceMutex);
        QMessageBox::warning(nullptr, "Already running", "UGVControlStation is already running.");
        return 1;
    }

    app.setStyle("Fusion");
    app.setPalette(Theme::darkPalette());

    auto consoleSink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
    consoleSink->set_pattern("[%T.%e] [%^%l%$] %v");
    auto logger = std::make_shared<spdlog::logger>("", consoleSink);
    logger->set_level(verbose ? spdlog::level::debug : spdlog::level::info);
    logger->flush_on(spdlog::level::trace);
    spdlog::set_default_logger(logger);

    std::string configPath = findConfig(argc, argv);
    if (configPath.empty())
        spdlog::warn("No config.json found — using defaults");

    AppConfig config = configPath.empty()
        ? AppConfig::defaults()
        : AppConfig::load(configPath);

    {
        QSettings s(SettingsKeys::kOrg, SettingsKeys::kApp);
        QString family = s.value(SettingsKeys::kFontFamily, "Segoe UI").toString();
        int     size   = s.value(SettingsKeys::kFontSize,   config.ui.fontSize).toInt();
        app.setFont(QFont(family, size));
    }
    AppState state;
    {
        QSettings s(SettingsKeys::kOrg, SettingsKeys::kApp);
        state.wheelSizePercent.store(s.value(SettingsKeys::kWheelSize, 100).toInt());
        for (int i = 0; i < KeyBindings::Count; ++i) {
            auto k1 = QString(SettingsKeys::kBindKey1Fmt).arg(i);
            auto k2 = QString(SettingsKeys::kBindKey2Fmt).arg(i);
            if (s.contains(k1)) state.keyBindings.actions[i].key1 = s.value(k1).toInt();
            if (s.contains(k2)) state.keyBindings.actions[i].key2 = s.value(k2).toInt();
        }
    }
    state.ugv = state.registry.create();
    state.registry.emplace<ControlState>(state.ugv);
    state.registry.emplace<TelemetryState>(state.ugv);
    state.registry.emplace<SafetyState>(state.ugv);
    state.registry.emplace<ConnectionState>(state.ugv);
    state.registry.get<ConnectionState>(state.ugv).baudrate = config.serial.baudrate;

    auto uiSink = std::make_shared<UiLogSink>(state.logs);
    uiSink->set_pattern("[%T] %v");
    uiSink->set_level(spdlog::level::debug);
    spdlog::default_logger()->sinks().push_back(uiSink);

    SerialWorker  worker(state, config);
    InputManager  inputManager;
    inputManager.addSource(std::make_unique<KeyboardInput>(state.keyBindings, config.input));
    inputManager.addSource(std::make_unique<XInputGamepad>(0, config.input));

    if (!config.serial.port.empty()) {
        worker.requestConnect(config.serial.port, config.serial.baudrate);
        std::lock_guard<std::mutex> lk(state.registryMutex);
        state.registry.get<ConnectionState>(state.ugv).portName = config.serial.port;
    }

    MainWindow window(state, config, worker, inputManager);
    window.showMaximized();

    worker.start();
    spdlog::info("UGV Control Station started — {}Hz worker, Qt6 UI", config.control.rateHz);

    int ret = app.exec();

    spdlog::info("UGV Control Station stopped.");
    return ret;
}
