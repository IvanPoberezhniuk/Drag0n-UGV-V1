#pragma once
#include <QWidget>
#include "core/AppState.h"

class QLabel;
class QProgressBar;

class TelemetryPanel : public QWidget {
public:
    explicit TelemetryPanel(AppState& state, QWidget* parent = nullptr);
    void refresh();

private:
    AppState& m_state;

    QLabel*       m_noDataLabel  = nullptr;
    QWidget*      m_dataWidget   = nullptr;
    QProgressBar* m_rssi1Bar     = nullptr;
    QProgressBar* m_rssi2Bar     = nullptr;
    QProgressBar* m_lqBar        = nullptr;
    QLabel*       m_batteryLabel = nullptr;
    QLabel*       m_speedLabel   = nullptr;
    QLabel*       m_headingLabel = nullptr;
};
