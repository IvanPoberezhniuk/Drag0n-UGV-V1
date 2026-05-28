#include "ui/MainWindow.h"
#include "ui/panels/ConnectionPanel.h"
#include "ui/panels/ControlPanel.h"
#include "ui/panels/TelemetryPanel.h"
#include "ui/panels/LogsPanel.h"
#include "ui/panels/LegendPanel.h"
#include "ui/panels/VideoPanel.h"
#include "ui/SettingsDialog.h"
#include "ui/SettingsKeys.h"
#include <QDockWidget>
#include <QCloseEvent>
#include <QEvent>
#include <QMenuBar>
#include <QAction>
#include <QApplication>
#include <QScreen>
#include <QSettings>
#include <QTimer>
#include <QShowEvent>

MainWindow::MainWindow(AppState& state, const AppConfig& config,
                       SerialWorker& worker, InputManager& inputManager,
                       QWidget* parent)
    : QMainWindow(parent)
    , m_state(state)
    , m_config(config)
    , m_worker(worker)
    , m_inputManager(inputManager)
{
    setWindowTitle("UGV Control Station");
    resize(1280, 720);
    setDockNestingEnabled(true);

    m_connection = new ConnectionPanel(m_state, m_worker, m_config, this);
    m_control    = new ControlPanel(m_state, this);
    m_telemetry  = new TelemetryPanel(m_state, this);
    m_logs       = new LogsPanel(m_state, this);
    m_legend     = new LegendPanel(m_state, this);
    m_video      = new VideoPanel(m_state, this);

    setCentralWidget(m_video);

    m_panels = { m_connection, m_control, m_telemetry, m_logs, m_legend, m_video };

    auto makeDock = [this](const QString& title, QWidget* w, Qt::DockWidgetArea area) {
        auto* dock = new QDockWidget(title, this);
        dock->setWidget(w);
        dock->setAllowedAreas(Qt::AllDockWidgetAreas);
        addDockWidget(area, dock);
        return dock;
    };

    // Left column: Connection (top) + Control (middle) + Legend (bottom)
    m_connDock    = makeDock("Connection", m_connection, Qt::LeftDockWidgetArea);
    m_controlDock = makeDock("Control",    m_control,    Qt::LeftDockWidgetArea);
    m_legendDock  = makeDock("Legend",     m_legend,     Qt::LeftDockWidgetArea);
    splitDockWidget(m_connDock,    m_controlDock, Qt::Vertical);
    splitDockWidget(m_controlDock, m_legendDock,  Qt::Vertical);

    // Bottom row: Telemetry (left) + Logs (right), full width
    m_telemetryDock = makeDock("Telemetry", m_telemetry, Qt::BottomDockWidgetArea);
    m_logsDock      = makeDock("Logs",      m_logs,      Qt::BottomDockWidgetArea);
    splitDockWidget(m_telemetryDock, m_logsDock, Qt::Horizontal);

    // Bottom spans full window width
    setCorner(Qt::BottomLeftCorner,  Qt::BottomDockWidgetArea);
    setCorner(Qt::BottomRightCorner, Qt::BottomDockWidgetArea);

    auto* fileMenu   = menuBar()->addMenu("&File");
    auto* quitAction = fileMenu->addAction("&Quit");
    quitAction->setShortcut(QKeySequence(Qt::ALT | Qt::Key_F4));
    connect(quitAction, &QAction::triggered, qApp, &QApplication::quit);

    auto* settingsMenu = menuBar()->addMenu("&Settings");
    auto* prefsAction  = settingsMenu->addAction("&Preferences...");
    prefsAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_Comma));
    connect(prefsAction, &QAction::triggered, this, &MainWindow::openSettings);

    auto* viewMenu = menuBar()->addMenu("&View");
    viewMenu->addAction(m_connDock->toggleViewAction());
    viewMenu->addAction(m_controlDock->toggleViewAction());
    viewMenu->addAction(m_legendDock->toggleViewAction());
    viewMenu->addSeparator();
    viewMenu->addAction(m_telemetryDock->toggleViewAction());
    viewMenu->addAction(m_logsDock->toggleViewAction());

    connect(&m_timer, &QTimer::timeout, this, [this]() { onTick(); });
    m_timer.start(static_cast<int>(m_config.ui.refreshIntervalMs));

    QSettings s(SettingsKeys::kOrg, SettingsKeys::kApp);
    if (s.contains(SettingsKeys::kGeometry))    restoreGeometry(s.value(SettingsKeys::kGeometry).toByteArray());
    if (s.contains(SettingsKeys::kWindowState)) {
        restoreState(s.value(SettingsKeys::kWindowState).toByteArray());
        m_hasRestoredLayout = true;
    }
}

void MainWindow::showEvent(QShowEvent* e) {
    QMainWindow::showEvent(e);
    if (!m_layoutDone && !m_hasRestoredLayout) {
        m_layoutDone = true;
        QTimer::singleShot(0, this, &MainWindow::applyDefaultLayout);
    }
}

void MainWindow::applyDefaultLayout() {
    QRect screen = QApplication::primaryScreen()->availableGeometry();
    int W = screen.width(), H = screen.height();

    m_connDock->setFixedWidth(W / 8);
    resizeDocks({m_telemetryDock, m_logsDock}, {W/2, W/2}, Qt::Horizontal);
    resizeDocks({m_telemetryDock},             {H / 5},    Qt::Vertical);

    QTimer::singleShot(200, this, [this]() {
        m_connDock->setMinimumWidth(80);
        m_connDock->setMaximumWidth(QWIDGETSIZE_MAX);
    });
}

void MainWindow::openSettings() {
    SettingsDialog dlg(m_state, this);
    dlg.exec();
}

void MainWindow::onTick() {
    m_inputManager.poll(m_state);
    for (auto* p : m_panels) p->refresh();
}

void MainWindow::closeEvent(QCloseEvent* e) {
    QSettings s(SettingsKeys::kOrg, SettingsKeys::kApp);
    s.setValue(SettingsKeys::kGeometry,    saveGeometry());
    s.setValue(SettingsKeys::kWindowState, saveState());

    m_timer.stop();
    m_worker.stop();
    e->accept();
}
