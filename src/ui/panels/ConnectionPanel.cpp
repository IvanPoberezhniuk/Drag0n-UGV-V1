#include "ui/panels/ConnectionPanel.h"
#include "io/SerialPort.h"
#include "core/ConnectionState.h"
#include "core/StateSnapshot.h"
#include "ui/RadioSettingsDialog.h"
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

    // Radio (ELRS TX module / receiver) settings; needs the link, so refresh()
    // enables it only while connected.
    m_settingsBtn = new QPushButton(this);
    m_settingsBtn->setIcon(QIcon(":/icons/settings.svg"));
    m_settingsBtn->setFixedSize(34, 30);
    m_settingsBtn->setEnabled(false);
    portRow->addWidget(m_settingsBtn);

    m_infoIcon = new HoverInfoIcon(this);
    portRow->addWidget(m_infoIcon);
    layout->addLayout(portRow);

    connect(m_connectBtn, &QPushButton::clicked, this, [this]() { onConnectClicked(); });
    connect(m_settingsBtn, &QPushButton::clicked, this, [this]() { openRadioSettings(); });

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

void ConnectionPanel::openRadioSettings() {
    RadioSettingsDialog dlg(m_state, m_worker, this);
    dlg.exec();
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

    const bool connected = conn.status == ConnectionStatus::Connected;
    m_settingsBtn->setEnabled(connected);
    m_settingsBtn->setToolTip(connected ? "Radio settings (TX module and receiver)"
                                        : "Radio settings (connect first)");

    m_infoIcon->setTooltipHtml(tooltipHtml("Connection", {
        {"Port", conn.portName.empty() ? QString("—") : QString::fromStdString(conn.portName)},
        {"Baud", QString::number(conn.baudrate)},
        {"Pkt/s", QString::number(conn.pktPerSec)},
    }));
}
