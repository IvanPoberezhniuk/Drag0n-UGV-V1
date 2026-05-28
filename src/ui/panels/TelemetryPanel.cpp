#include "ui/panels/TelemetryPanel.h"
#include "core/TelemetryState.h"
#include "core/StateSnapshot.h"
#include "ui/Theme.h"
#include <QVBoxLayout>
#include <QFormLayout>
#include <QLabel>
#include <QProgressBar>

TelemetryPanel::TelemetryPanel(AppState& state, QWidget* parent)
    : IPanel(parent), m_state(state)
{
    auto* layout = new QVBoxLayout(this);

    m_noDataLabel = new QLabel("No telemetry data\n(waiting for CRSF frames from RX)", this);
    m_noDataLabel->setAlignment(Qt::AlignCenter);
    m_noDataLabel->setStyleSheet(Theme::colorSS(Theme::textMuted));
    layout->addWidget(m_noDataLabel);

    m_dataWidget = new QWidget(this);
    auto* dataLayout = new QFormLayout(m_dataWidget);
    dataLayout->setRowWrapPolicy(QFormLayout::DontWrapRows);

    m_rssi1Bar = new QProgressBar(this);
    m_rssi1Bar->setRange(0, 100);
    m_rssi1Bar->setFormat("%v dBm");
    dataLayout->addRow("RSSI 1:", m_rssi1Bar);

    m_rssi2Bar = new QProgressBar(this);
    m_rssi2Bar->setRange(0, 100);
    m_rssi2Bar->setFormat("%v dBm");
    dataLayout->addRow("RSSI 2:", m_rssi2Bar);

    m_lqBar = new QProgressBar(this);
    m_lqBar->setRange(0, 100);
    m_lqBar->setFormat("%v%");
    dataLayout->addRow("LQ:", m_lqBar);

    m_batteryLabel = new QLabel("—", this);
    m_speedLabel   = new QLabel("—", this);
    m_headingLabel = new QLabel("—", this);
    dataLayout->addRow("Battery:", m_batteryLabel);
    dataLayout->addRow("Speed:",   m_speedLabel);
    dataLayout->addRow("Heading:", m_headingLabel);

    layout->addWidget(m_dataWidget);
    layout->addStretch();

    m_dataWidget->hide();
}

void TelemetryPanel::refresh() {
    auto telem = snapshot<TelemetryState>(m_state);

    if (!telem.valid) {
        m_noDataLabel->show();
        m_dataWidget->hide();
        return;
    }

    m_noDataLabel->hide();
    m_dataWidget->show();

    m_rssi1Bar->setValue(telem.rssi1);
    m_rssi2Bar->setValue(telem.rssi2);
    m_lqBar->setValue(telem.lq);

    m_batteryLabel->setText(QString::number(static_cast<double>(telem.batteryVoltage), 'f', 2) + " V");
    m_speedLabel->setText(QString::number(static_cast<double>(telem.speed), 'f', 1) + " m/s");
    m_headingLabel->setText(QString::number(static_cast<double>(telem.heading), 'f', 1) + " deg");
}
