#include "ui/panels/TelemetryPanel.h"
#include "core/TelemetryState.h"
#include "core/StateSnapshot.h"
#include "ui/Theme.h"
#include "ui/widgets/LiveDot.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QLabel>
#include <QProgressBar>
#include <QSizePolicy>

TelemetryPanel::TelemetryPanel(AppState& state, QWidget* parent)
    : IPanel(parent), m_state(state)
{
    auto* layout = new QVBoxLayout(this);

    m_noDataLabel = new QLabel("No telemetry data\n(waiting for CRSF frames from RX)", this);
    m_noDataLabel->setAlignment(Qt::AlignCenter);
    m_noDataLabel->setStyleSheet(Theme::colorSS(Theme::textMuted));
    layout->addWidget(m_noDataLabel);

    m_dataWidget = new QWidget(this);
    auto* dataLayout = new QVBoxLayout(m_dataWidget);

    auto* radioGroup = new QGroupBox(m_dataWidget);
    auto* radioLayout = new QFormLayout(radioGroup);
    radioLayout->setRowWrapPolicy(QFormLayout::DontWrapRows);

    auto* radioTitleRow = new QHBoxLayout;
    m_radioLiveDot = new LiveDot(radioGroup);
    auto* radioTitleLabel = new QLabel("Radio link", radioGroup);
    QFont radioTitleFont = radioTitleLabel->font();
    radioTitleFont.setBold(true);
    radioTitleLabel->setFont(radioTitleFont);
    radioTitleRow->addWidget(radioTitleLabel);
    radioTitleRow->addWidget(m_radioLiveDot, 0, Qt::AlignVCenter);
    radioTitleRow->addStretch();
    radioLayout->addRow(radioTitleRow);

    m_rssi1Bar = new QProgressBar(this);
    m_rssi1Bar->setRange(0, 100);
    m_rssi1Bar->setFormat("%v dBm");
    radioLayout->addRow("RSSI 1:", m_rssi1Bar);

    m_rssi2Bar = new QProgressBar(this);
    m_rssi2Bar->setRange(0, 100);
    m_rssi2Bar->setFormat("%v dBm");
    radioLayout->addRow("RSSI 2:", m_rssi2Bar);

    m_lqBar = new QProgressBar(this);
    m_lqBar->setRange(0, 100);
    m_lqBar->setFormat("%v%");
    radioLayout->addRow("Link quality:", m_lqBar);

    auto* bmsGroup = new QGroupBox(m_dataWidget);
    auto* bmsLayout = new QVBoxLayout(bmsGroup);

    auto* bmsTitleRow = new QHBoxLayout;
    m_bmsLiveDot = new LiveDot(bmsGroup);
    auto* bmsTitleLabel = new QLabel("JK BMS", bmsGroup);
    QFont bmsTitleFont = bmsTitleLabel->font();
    bmsTitleFont.setBold(true);
    bmsTitleLabel->setFont(bmsTitleFont);
    bmsTitleRow->addWidget(bmsTitleLabel);
    bmsTitleRow->addWidget(m_bmsLiveDot, 0, Qt::AlignVCenter);
    bmsTitleRow->addStretch();
    bmsLayout->addLayout(bmsTitleRow);

    auto* bmsPackGroup = new QGroupBox("Pack", bmsGroup);
    auto* bmsPackLayout = new QFormLayout(bmsPackGroup);
    bmsPackLayout->setRowWrapPolicy(QFormLayout::DontWrapRows);
    bmsPackLayout->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);

    auto* bmsCellsGroup = new QGroupBox("Cells", bmsGroup);
    auto* bmsCellsLayout = new QFormLayout(bmsCellsGroup);
    bmsCellsLayout->setRowWrapPolicy(QFormLayout::DontWrapRows);
    bmsCellsLayout->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);

    m_bmsSocLabel = new QLabel("—", this);
    m_bmsVoltageLabel = new QLabel("—", this);
    m_bmsCurrentLabel = new QLabel("—", this);
    m_bmsCapacityLabel = new QLabel("—", this);
    m_bmsCellsLabel = new QLabel("—", this);
    m_bmsDeltaLabel = new QLabel("—", this);
    m_bmsCellVoltLabel = new QLabel("—", this);
    m_bmsTempLabel = new QLabel("—", this);

    for (QLabel* valueLabel : {m_bmsSocLabel, m_bmsVoltageLabel,
                               m_bmsCurrentLabel, m_bmsCapacityLabel, m_bmsCellsLabel,
                               m_bmsDeltaLabel, m_bmsCellVoltLabel, m_bmsTempLabel}) {
        valueLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        valueLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    }
    bmsPackLayout->addRow("Battery:", m_bmsSocLabel);
    bmsPackLayout->addRow("Voltage:", m_bmsVoltageLabel);
    bmsPackLayout->addRow("Current:", m_bmsCurrentLabel);
    bmsPackLayout->addRow("Capacity:", m_bmsCapacityLabel);
    bmsPackLayout->addRow("Temperature range:", m_bmsTempLabel);
    bmsCellsLayout->addRow("Voltage range:", m_bmsCellsLabel);
    bmsCellsLayout->addRow("Voltage delta:", m_bmsDeltaLabel);
    bmsCellsLayout->addRow("Per-cell:", m_bmsCellVoltLabel);

    bmsLayout->addWidget(bmsPackGroup);
    bmsLayout->addWidget(bmsCellsGroup);

    auto* navigationGroup = new QGroupBox(m_dataWidget);
    auto* navigationLayout = new QFormLayout(navigationGroup);
    navigationLayout->setRowWrapPolicy(QFormLayout::DontWrapRows);
    navigationLayout->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);

    auto* navTitleLabel = new QLabel("Navigation", navigationGroup);
    QFont navTitleFont = navTitleLabel->font();
    navTitleFont.setBold(true);
    navTitleLabel->setFont(navTitleFont);
    navigationLayout->addRow(navTitleLabel);

    m_speedLabel   = new QLabel("—", this);
    m_headingLabel = new QLabel("—", this);
    for (QLabel* valueLabel : {m_speedLabel, m_headingLabel}) {
        valueLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        valueLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    }
    navigationLayout->addRow("Speed:", m_speedLabel);
    navigationLayout->addRow("Heading:", m_headingLabel);

    dataLayout->addWidget(radioGroup);
    dataLayout->addWidget(bmsGroup);
    dataLayout->addWidget(navigationGroup);

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

    m_radioLiveDot->setActive(true);
    m_bmsLiveDot->setActive(telem.bmsValid);

    m_rssi1Bar->setValue(telem.rssi1);
    m_rssi2Bar->setValue(telem.rssi2);
    m_lqBar->setValue(telem.lq);

    if (telem.bmsValid) {
        m_bmsSocLabel->setText(QString("%1 %").arg(telem.batterySocPct));
        m_bmsVoltageLabel->setText(
            QString("%1 V").arg(static_cast<double>(telem.batteryVoltage), 0, 'f', 3));
        m_bmsCurrentLabel->setText(
            QString("%1 A").arg(static_cast<double>(telem.batteryCurrent), 0, 'f', 3));
        m_bmsCapacityLabel->setText(
            QString("%1 / %2 Ah")
                .arg(static_cast<double>(telem.batteryRemainingAh), 0, 'f', 3)
                .arg(static_cast<double>(telem.batteryFullAh), 0, 'f', 3));
        m_bmsCellsLabel->setText(
            QString("%1–%2 mV").arg(telem.batteryCellMinMv).arg(telem.batteryCellMaxMv));
        m_bmsDeltaLabel->setText(QString("%1 mV").arg(telem.batteryCellDeltaMv));
        m_bmsCellVoltLabel->setText(
            QString("1: %1  2: %2  3: %3  4: %4 mV")
                .arg(telem.batteryCellMv[0]).arg(telem.batteryCellMv[1])
                .arg(telem.batteryCellMv[2]).arg(telem.batteryCellMv[3]));
        m_bmsTempLabel->setText(
            QString("%1…%2 °C").arg(telem.batteryTempLowC).arg(telem.batteryTempHighC));
    } else {
        for (QLabel* label : {m_bmsSocLabel, m_bmsVoltageLabel,
                              m_bmsCurrentLabel, m_bmsCapacityLabel,
                              m_bmsCellsLabel, m_bmsDeltaLabel,
                              m_bmsCellVoltLabel, m_bmsTempLabel}) {
            label->setText("—");
        }
    }
    m_speedLabel->setText(QString::number(static_cast<double>(telem.speed), 'f', 1) + " m/s");
    m_headingLabel->setText(QString::number(static_cast<double>(telem.heading), 'f', 1) + " deg");
}
