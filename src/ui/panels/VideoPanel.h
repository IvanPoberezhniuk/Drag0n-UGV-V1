#pragma once
#include "ui/IPanel.h"
#include "core/AppState.h"
#include "io/VideoWorker.h"
#include <QImage>
#include <random>

class WheelPanel;
class CompassBar;
class DashboardBar;
class HudCrosshair;
class NoSignalBadge;

class VideoPanel : public IPanel {
    Q_OBJECT
public:
    explicit VideoPanel(AppState& state, VideoWorker& videoWorker, QWidget* parent = nullptr);
    void refresh() override;

protected:
    void paintEvent(QPaintEvent*) override;
    void resizeEvent(QResizeEvent*) override;
    void wheelEvent(QWheelEvent*) override;

private:
    AppState&     m_state;
    VideoWorker&  m_videoWorker;
    WheelPanel*   m_wheels    = nullptr;
    CompassBar*   m_compass   = nullptr;
    DashboardBar* m_dashboard = nullptr;
    HudCrosshair* m_hud       = nullptr;
    NoSignalBadge* m_noSignalBadge = nullptr;
    QImage        m_noise;
    std::mt19937  m_rng{ std::random_device{}() };

    void repositionWheels();
    void repositionCompass();
    void repositionDashboard();
    void repositionHud();
    void repositionNoSignalBadge();
    void generateNoise();
};
