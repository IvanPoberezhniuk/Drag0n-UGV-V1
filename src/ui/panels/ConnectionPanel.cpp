#include "ui/panels/ConnectionPanel.h"
#include "io/SerialPort.h"
#include "core/ConnectionState.h"
#include "core/StateSnapshot.h"
#include "ui/Theme.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QComboBox>
#include <QLineEdit>
#include <QPushButton>
#include <QLabel>

static QString statusText(ConnectionStatus s) {
    switch (s) {
        case ConnectionStatus::Disconnected: return "Disconnected";
        case ConnectionStatus::Connecting:   return "Connecting...";
        case ConnectionStatus::Connected:    return "Connected";
        case ConnectionStatus::Error:        return "Error";
    }
    return "Unknown";
}

static QString statusStyle(ConnectionStatus s) {
    switch (s) {
        case ConnectionStatus::Disconnected: return Theme::colorSS(Theme::textMuted);
        case ConnectionStatus::Connecting:   return Theme::colorSS(Theme::warningYellow);
        case ConnectionStatus::Connected:    return Theme::colorSS(Theme::successGreen);
        case ConnectionStatus::Error:        return Theme::colorSS(Theme::errorRed);
    }
    return "";
}

ConnectionPanel::ConnectionPanel(AppState& state, SerialWorker& worker,
                                 const AppConfig& config, QWidget* parent)
    : IPanel(parent), m_state(state), m_worker(worker)
{
    auto* layout = new QVBoxLayout(this);

    m_statusLabel = new QLabel("● Disconnected", this);
    layout->addWidget(m_statusLabel);

    auto* portRow = new QHBoxLayout;
    portRow->addWidget(new QLabel("Port:", this));
    m_portCombo = new QComboBox(this);
    m_portCombo->addItem("auto");
    portRow->addWidget(m_portCombo, 1);
    auto* refreshBtn = new QPushButton("Refresh", this);
    portRow->addWidget(refreshBtn);
    layout->addLayout(portRow);

    auto* baudRow = new QHBoxLayout;
    baudRow->addWidget(new QLabel("Baud:", this));
    m_baudEdit = new QLineEdit(QString::number(config.serial.baudrate), this);
    m_baudEdit->setMaximumWidth(100);
    baudRow->addWidget(m_baudEdit);
    baudRow->addStretch();
    layout->addLayout(baudRow);

    m_connectBtn = new QPushButton("Connect", this);
    layout->addWidget(m_connectBtn);

    layout->addSpacing(8);

    m_portInfoLabel = new QLabel("Port: —", this);
    m_baudInfoLabel = new QLabel("Baud: 0", this);
    m_pktLabel      = new QLabel("Pkt/s: 0", this);
    layout->addWidget(m_portInfoLabel);
    layout->addWidget(m_baudInfoLabel);
    layout->addWidget(m_pktLabel);
    layout->addStretch();

    connect(m_connectBtn, &QPushButton::clicked, this, [this]() { onConnectClicked(); });
    connect(refreshBtn,   &QPushButton::clicked, this, [this]() { onRefreshClicked(); });

    onRefreshClicked();
}

void ConnectionPanel::onRefreshClicked() {
    auto ports = SerialPort::listAll();
    m_portCombo->clear();
    m_portCombo->addItem("auto");
    for (const auto& p : ports)
        m_portCombo->addItem(QString::fromStdString(p));
}

void ConnectionPanel::onConnectClicked() {
    auto conn = snapshot<ConnectionState>(m_state);

    bool active = conn.status == ConnectionStatus::Connected ||
                  conn.status == ConnectionStatus::Connecting;

    if (active) {
        m_worker.requestDisconnect();
    } else {
        QString portStr = m_portCombo->currentText();
        std::string port = (portStr == "auto") ? "auto" : portStr.toStdString();
        uint32_t baud = static_cast<uint32_t>(m_baudEdit->text().toInt());
        if (baud == 0) baud = 420000;
        m_worker.requestConnect(port, baud);
    }
}

void ConnectionPanel::refresh() {
    auto conn = snapshot<ConnectionState>(m_state);

    QString label = "● " + statusText(conn.status);
    if (!conn.errorMessage.empty())
        label += "   " + QString::fromStdString(conn.errorMessage);
    m_statusLabel->setText(label);
    m_statusLabel->setStyleSheet(statusStyle(conn.status));

    bool active = conn.status == ConnectionStatus::Connected ||
                  conn.status == ConnectionStatus::Connecting;
    m_connectBtn->setText(active ? "Disconnect" : "Connect");

    m_portInfoLabel->setText("Port: " + (conn.portName.empty()
        ? QString("—") : QString::fromStdString(conn.portName)));
    m_baudInfoLabel->setText("Baud: " + QString::number(conn.baudrate));
    m_pktLabel->setText("Pkt/s: " + QString::number(conn.pktPerSec));
}
