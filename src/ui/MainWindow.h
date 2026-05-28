#pragma once
#include <QMainWindow>
#include <QTimer>
#include <vector>
#include "core/AppState.h"
#include "config/AppConfig.h"
#include "io/SerialWorker.h"
#include "input/InputManager.h"
#include "ui/IPanel.h"

class ConnectionPanel;
class ControlPanel;
class TelemetryPanel;
class LogsPanel;
class LegendPanel;
class VideoPanel;
class QDockWidget;

class MainWindow : public QMainWindow {
public:
    MainWindow(AppState& state, const AppConfig& config,
               SerialWorker& worker, InputManager& inputManager,
               QWidget* parent = nullptr);

protected:
    void closeEvent(QCloseEvent* e) override;
    void showEvent(QShowEvent* e) override;

private slots:
    void openSettings();

private:
    void onTick();
    void applyDefaultLayout();

    AppState&        m_state;
    const AppConfig& m_config;
    SerialWorker&    m_worker;
    InputManager&    m_inputManager;

    ConnectionPanel* m_connection = nullptr;
    ControlPanel*    m_control    = nullptr;
    TelemetryPanel*  m_telemetry  = nullptr;
    LogsPanel*       m_logs       = nullptr;
    LegendPanel*     m_legend     = nullptr;
    VideoPanel*      m_video      = nullptr;

    QDockWidget* m_connDock      = nullptr;
    QDockWidget* m_controlDock   = nullptr;
    QDockWidget* m_telemetryDock = nullptr;
    QDockWidget* m_logsDock      = nullptr;
    QDockWidget* m_legendDock    = nullptr;

    std::vector<IPanel*> m_panels;

    QTimer m_timer;
    bool   m_layoutDone        = false;
    bool   m_hasRestoredLayout = false;
};
