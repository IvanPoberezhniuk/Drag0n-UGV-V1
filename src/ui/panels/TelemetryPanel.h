#pragma once
#include "ui/IPanel.h"
#include "core/AppState.h"

class QLabel;
class QProgressBar;
class LiveDot;

class TelemetryPanel : public IPanel {
public:
    explicit TelemetryPanel(AppState& state, QWidget* parent = nullptr);
    void refresh() override;

private:
    AppState& m_state;

    LiveDot*      m_radioLiveDot = nullptr;
    LiveDot*      m_bmsLiveDot   = nullptr;
    QLabel*       m_noDataLabel  = nullptr;
    QWidget*      m_dataWidget   = nullptr;
    QProgressBar* m_rssi1Bar     = nullptr;
    QProgressBar* m_rssi2Bar     = nullptr;
    QProgressBar* m_lqBar        = nullptr;
    QLabel*       m_bmsSocLabel = nullptr;
    QLabel*       m_bmsVoltageLabel = nullptr;
    QLabel*       m_bmsCurrentLabel = nullptr;
    QLabel*       m_bmsCapacityLabel = nullptr;
    QLabel*       m_bmsCellsLabel = nullptr;
    QLabel*       m_bmsDeltaLabel = nullptr;
    QLabel*       m_bmsCellVoltLabel = nullptr;
    QLabel*       m_bmsTempLabel = nullptr;
    QLabel*       m_speedLabel   = nullptr;
    QLabel*       m_headingLabel = nullptr;
};
