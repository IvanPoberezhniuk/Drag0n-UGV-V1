#pragma once
#include <QMainWindow>
#include <QTimer>
#include "core/AppState.h"
#include "config/AppConfig.h"
#include "io/SerialWorker.h"
#include "input/KeyboardInput.h"

class ConnectionPanel;
class ControlPanel;
class TelemetryPanel;
class LogsPanel;

class MainWindow : public QMainWindow {
public:
    MainWindow(AppState& state, const AppConfig& config,
               SerialWorker& worker, KeyboardInput& keyboard,
               QWidget* parent = nullptr);

protected:
    void closeEvent(QCloseEvent* e) override;

private:
    void onTick();

    AppState&        m_state;
    const AppConfig& m_config;
    SerialWorker&    m_worker;
    KeyboardInput&   m_keyboard;

    ConnectionPanel* m_connection = nullptr;
    ControlPanel*    m_control    = nullptr;
    TelemetryPanel*  m_telemetry  = nullptr;
    LogsPanel*       m_logs       = nullptr;

    QTimer m_timer;
};
