#include "ui/MainWindow.h"
#include "ui/panels/ConnectionPanel.h"
#include "ui/panels/ControlPanel.h"
#include "ui/panels/TelemetryPanel.h"
#include "ui/panels/LogsPanel.h"
#include <QDockWidget>
#include <QCloseEvent>
#include <QMenuBar>
#include <QAction>
#include <QApplication>
#include <QSettings>

MainWindow::MainWindow(AppState& state, const AppConfig& config,
                       SerialWorker& worker, KeyboardInput& keyboard,
                       QWidget* parent)
    : QMainWindow(parent)
    , m_state(state)
    , m_config(config)
    , m_worker(worker)
    , m_keyboard(keyboard)
{
    setWindowTitle("UGV Control Station");
    resize(1280, 720);
    setDockNestingEnabled(true);

    m_connection = new ConnectionPanel(m_state, m_worker, m_config, this);
    m_control    = new ControlPanel(m_state, this);
    m_telemetry  = new TelemetryPanel(m_state, this);
    m_logs       = new LogsPanel(m_state, this);

    auto makeDock = [this](const QString& title, QWidget* w, Qt::DockWidgetArea area) {
        auto* dock = new QDockWidget(title, this);
        dock->setWidget(w);
        dock->setAllowedAreas(Qt::AllDockWidgetAreas);
        addDockWidget(area, dock);
        return dock;
    };

    makeDock("Connection", m_connection, Qt::LeftDockWidgetArea);
    makeDock("Control",    m_control,    Qt::LeftDockWidgetArea);
    makeDock("Telemetry",  m_telemetry,  Qt::RightDockWidgetArea);
    makeDock("Logs",       m_logs,       Qt::BottomDockWidgetArea);

    auto* fileMenu = menuBar()->addMenu("&File");
    auto* quitAction = fileMenu->addAction("&Quit");
    quitAction->setShortcut(QKeySequence(Qt::ALT | Qt::Key_F4));
    connect(quitAction, &QAction::triggered, qApp, &QApplication::quit);

    connect(&m_timer, &QTimer::timeout, this, [this]() { onTick(); });
    m_timer.start(30);

    QSettings s("UGVControlStation", "connectionApp");
    if (s.contains("geometry"))    restoreGeometry(s.value("geometry").toByteArray());
    if (s.contains("windowState")) restoreState(s.value("windowState").toByteArray());
}

void MainWindow::onTick() {
    m_keyboard.poll(m_state);

    m_connection->refresh();
    m_control->refresh();
    m_telemetry->refresh();
    m_logs->refresh();
}

void MainWindow::closeEvent(QCloseEvent* e) {
    QSettings s("UGVControlStation", "connectionApp");
    s.setValue("geometry",    saveGeometry());
    s.setValue("windowState", saveState());

    m_timer.stop();
    m_worker.stop();
    e->accept();
}
