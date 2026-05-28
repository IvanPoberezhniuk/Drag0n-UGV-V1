#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include <QIcon>
#include <QApplication>
#include <QPalette>
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
    QPalette dark;
    dark.setColor(QPalette::Window,          QColor(45,  45,  45));
    dark.setColor(QPalette::WindowText,      Qt::white);
    dark.setColor(QPalette::Base,            QColor(30,  30,  30));
    dark.setColor(QPalette::AlternateBase,   QColor(53,  53,  53));
    dark.setColor(QPalette::ToolTipBase,     Qt::white);
    dark.setColor(QPalette::ToolTipText,     Qt::white);
    dark.setColor(QPalette::Text,            Qt::white);
    dark.setColor(QPalette::Button,          QColor(53,  53,  53));
    dark.setColor(QPalette::ButtonText,      Qt::white);
    dark.setColor(QPalette::BrightText,      Qt::red);
    dark.setColor(QPalette::Highlight,       QColor(42, 130, 218));
    dark.setColor(QPalette::HighlightedText, Qt::black);
    dark.setColor(QPalette::Link,            QColor(42, 130, 218));
    app.setPalette(dark);

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
        QSettings s("UGVControlStation", "UGVControlStation");
        QString family = s.value("ui/fontFamily", "Segoe UI").toString();
        int     size   = s.value("ui/fontSize",   config.ui.fontSize).toInt();
        app.setFont(QFont(family, size));
    }
    AppState state;
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
    inputManager.addSource(std::make_unique<KeyboardInput>());
    inputManager.addSource(std::make_unique<XInputGamepad>(0));

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
