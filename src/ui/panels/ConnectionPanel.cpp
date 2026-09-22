#include "ui/panels/ConnectionPanel.h"
#include "io/SerialPort.h"
#include "core/ConnectionState.h"
#include "core/StateSnapshot.h"
#include "ui/Theme.h"
#include "ui/TooltipHtml.h"
#include "ui/widgets/HoverInfoIcon.h"
#include "ui/widgets/LiveDot.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QComboBox>
#include <QPushButton>
#include <QIcon>
#include <QLabel>
#include <functional>

namespace {

// Re-scans available ports right before the dropdown opens, replacing the
// separate "Refresh" button.
class PortComboBox : public QComboBox {
public:
    using QComboBox::QComboBox;
    std::function<void()> onAboutToShow;

protected:
    void showPopup() override {
        if (onAboutToShow) onAboutToShow();
        QComboBox::showPopup();
    }
};

} // namespace

struct StatusInfo { const char* text; const QColor& color; };

static StatusInfo statusInfo(ConnectionStatus s) {
    switch (s) {
        case ConnectionStatus::Disconnected: return { "Disconnected", Theme::textMuted     };
        case ConnectionStatus::Connecting:   return { "Connecting...", Theme::warningYellow };
        case ConnectionStatus::Connected:    return { "Connected",    Theme::accent        };
        case ConnectionStatus::Error:        return { "Error",        Theme::errorRed      };
    }
    return { "Unknown", Theme::textMuted };
}

ConnectionPanel::ConnectionPanel(AppState& state, SerialWorker& worker,
                                 const AppConfig& /*config*/, QWidget* parent)
    : IPanel(parent), m_state(state), m_worker(worker)
{
    // Baud rate now comes from AppState::serialBaudrate (Settings >
    // Connection), not per-config-file at construction time.
    auto* layout = new QVBoxLayout(this);

    auto* statusRow = new QHBoxLayout;
    m_statusLabel = new QLabel("Disconnected", this);
    m_statusDot = new LiveDot(this);
    statusRow->addWidget(m_statusLabel);
    statusRow->addWidget(m_statusDot, 0, Qt::AlignVCenter);
    statusRow->addStretch();
    layout->addLayout(statusRow);

    auto* portRow = new QHBoxLayout;
    auto* portCombo = new PortComboBox(this);
    portCombo->addItem("auto");
    portCombo->onAboutToShow = [this]() { refreshPortList(); };
    m_portCombo = portCombo;
    portRow->addWidget(m_portCombo, 1);

    m_connectBtn = new QPushButton(this);
    m_connectBtn->setIcon(QIcon(":/icons/plug-off.svg"));
    m_connectBtn->setFixedSize(34, 30);
    portRow->addWidget(m_connectBtn);

    m_infoIcon = new HoverInfoIcon(this);
    portRow->addWidget(m_infoIcon);
    layout->addLayout(portRow);

    connect(m_connectBtn, &QPushButton::clicked, this, [this]() { onConnectClicked(); });

    refreshPortList();
}

void ConnectionPanel::refreshPortList() {
    QString current = m_portCombo->currentText();
    auto ports = SerialPort::listAll();
    m_portCombo->clear();
    m_portCombo->addItem("auto");
    for (const auto& p : ports)
        m_portCombo->addItem(QString::fromStdString(p));
    int idx = m_portCombo->findText(current);
    m_portCombo->setCurrentIndex(idx >= 0 ? idx : 0);
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
        m_worker.requestConnect(port, m_state.serialBaudrate);
    }
}

void ConnectionPanel::refresh() {
    auto conn = snapshot<ConnectionState>(m_state);

    auto [text, color] = statusInfo(conn.status);
    QString label = text;
    if (!conn.errorMessage.empty())
        label += "   " + QString::fromStdString(conn.errorMessage);
    m_statusLabel->setText(label);
    m_statusLabel->setStyleSheet(Theme::colorSS(color));
    m_statusDot->setActive(conn.status == ConnectionStatus::Connected);

    bool active = conn.status == ConnectionStatus::Connected ||
                  conn.status == ConnectionStatus::Connecting;
    m_connectBtn->setIcon(QIcon(active ? ":/icons/plug-connected.svg" : ":/icons/plug-off.svg"));
    m_connectBtn->setToolTip(active ? "Disconnect" : "Connect");

    m_infoIcon->setTooltipHtml(tooltipHtml("Connection", {
        {"Port", conn.portName.empty() ? QString("—") : QString::fromStdString(conn.portName)},
        {"Baud", QString::number(conn.baudrate)},
        {"Pkt/s", QString::number(conn.pktPerSec)},
    }));
}
