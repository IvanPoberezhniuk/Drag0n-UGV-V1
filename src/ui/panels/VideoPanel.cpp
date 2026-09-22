#include "ui/panels/VideoPanel.h"
#include "ui/panels/WheelPanel.h"
#include "ui/CompassBar.h"
#include "ui/DashboardBar.h"
#include "ui/HudCrosshair.h"
#include "ui/Theme.h"
#include "core/TelemetryState.h"
#include "core/ControlState.h"
#include "core/SafetyState.h"
#include "core/SafetyStateName.h"
#include <QPainter>
#include <QWheelEvent>
#include <algorithm>
#include <mutex>

namespace {
QString formatUptime(uint32_t uptimeMs) {
    const uint32_t totalSeconds = uptimeMs / 1000u;
    const uint32_t hours = totalSeconds / 3600u;
    const uint32_t minutes = (totalSeconds % 3600u) / 60u;
    const uint32_t seconds = totalSeconds % 60u;
    if (hours > 0u) {
        return QString("%1h %2m %3s").arg(hours).arg(minutes).arg(seconds);
    }
    if (minutes > 0u) {
        return QString("%1m %2s").arg(minutes).arg(seconds);
    }
    return QString("%1s").arg(seconds);
}

QString formatBytes(uint32_t bytes) {
    if (bytes >= 1024u * 1024u) {
        return QString::number(bytes / (1024.0 * 1024.0), 'f', 2) + " MB";
    }
    if (bytes >= 1024u) {
        return QString::number(bytes / 1024.0, 'f', 1) + " KB";
    }
    return QString("%1 B").arg(bytes);
}
} // namespace

static constexpr int kNoiseW = 480;
static constexpr int kNoiseH = 360;
static constexpr int kDashboardH = 44;    // DashboardBar's fixed logical height
static constexpr int kBottomOverbleed = 4; // extends past the panel's bottom edge so
                                            // HiDPI/rounding can't leave a seam below
                                            // it -- Qt clips child widgets to the
                                            // parent rect, so this is never visible

VideoPanel::VideoPanel(AppState& state, QWidget* parent)
    : IPanel(parent), m_state(state)
{
    setMinimumSize(320, 240);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    m_noise = QImage(kNoiseW, kNoiseH, QImage::Format_Grayscale8);

    m_wheels = new WheelPanel(state, this);
    m_wheels->show();

    m_compass = new CompassBar(this);
    m_compass->show();
    repositionCompass();

    m_dashboard = new DashboardBar(this);
    m_dashboard->show();
    repositionDashboard();

    m_hud = new HudCrosshair(this);
    m_hud->show();
    repositionHud();
}

void VideoPanel::generateNoise() {
    uchar* data  = m_noise.bits();
    int    total = kNoiseW * kNoiseH;
    // Pack 4 pixels per rng call — remap 0-255 → 85-180 (muted grey range)
    int i = 0;
    for (; i + 3 < total; i += 4) {
        uint32_t r = m_rng();
        data[i]   = 85 + ((r        & 0xFF) * 95u >> 8);
        data[i+1] = 85 + (((r >> 8) & 0xFF) * 95u >> 8);
        data[i+2] = 85 + (((r >>16) & 0xFF) * 95u >> 8);
        data[i+3] = 85 + (((r >>24) & 0xFF) * 95u >> 8);
    }
    for (; i < total; ++i)
        data[i] = 85 + ((m_rng() & 0xFF) * 95u >> 8);
}

void VideoPanel::refresh() {
    if (m_state.whiteNoiseEnabled.load()) {
        generateNoise();
    }
    m_wheels->refresh();
    repositionWheels();

    {
        std::lock_guard<std::mutex> lk(m_state.registryMutex);
        auto& telem  = m_state.registry.get<TelemetryState>(m_state.ugv);
        auto& ctrl   = m_state.registry.get<ControlState>(m_state.ugv);
        auto& safety = m_state.registry.get<SafetyState>(m_state.ugv);
        m_compass->setHeading(telem.heading, telem.valid);
        m_dashboard->setCruise(ctrl.cruiseEnabled, ctrl.cruiseSpeed);
        m_dashboard->setEstop(ctrl.estop || safety.estopLatched);
        m_dashboard->setLights(ctrl.lightsOn);

        // ESP/STM32 link health: real data, from the CRSF telemetry link
        // itself and the diagnostic frame's per-node age tracking.
        m_dashboard->setEspOk(telem.valid);
        m_dashboard->setStmLeftOk(telem.stmLeftOnline);
        m_dashboard->setStmRightOk(telem.stmRightOnline);

        m_dashboard->setEspDetail(telem.valid
            ? DashboardBar::DetailRows{
                  {"RSSI", QString("%1 / %2 dBm").arg(telem.rssi1).arg(telem.rssi2)},
                  {"Link quality", QString("%1%").arg(telem.lq)},
                  {"Uptime", formatUptime(telem.espUptimeMs)},
                  {"Free heap", formatBytes(telem.espFreeHeapBytes)}}
            : DashboardBar::DetailRows{});
        auto stmDetail = [](uint8_t state, uint8_t faultMask, uint16_t ageMs,
                             uint32_t uptimeMs, uint16_t stackFreeBytes) -> DashboardBar::DetailRows {
            if (ageMs == 0xFFFFu) {
                return {{"State", safetyStateName(state)}, {"Telemetry", "never received"}};
            }
            return {
                {"State", safetyStateName(state)},
                {"Faults", QString("0x%1").arg(faultMask, 2, 16, QChar('0'))},
                {"Age", QString("%1 ms").arg(ageMs)},
                {"Uptime", formatUptime(uptimeMs)},
                {"Free stack", formatBytes(stackFreeBytes)}};
        };
        m_dashboard->setStmLeftDetail(stmDetail(telem.stmLeftState, telem.stmLeftFaultMask, telem.stmLeftAgeMs,
                                                 telem.stmLeftUptimeMs, telem.stmLeftStackFreeBytes));
        m_dashboard->setStmRightDetail(stmDetail(telem.stmRightState, telem.stmRightFaultMask, telem.stmRightAgeMs,
                                                  telem.stmRightUptimeMs, telem.stmRightStackFreeBytes));

        // GPS/velocity-sensor/speaker/camera have no telemetry source yet
        // (hardware not integrated) -- wire these up when that lands.
        m_dashboard->setGpsOk(false);
        m_dashboard->setVelocitySensorOk(false);
        m_dashboard->setSpeakerOk(false);
        m_dashboard->setCameraStreaming(false);

        m_hud->setThrottle(ctrl.throttle);
        m_hud->setBatteryLevel(
            static_cast<float>(telem.batterySocPct) / 100.0f,
            telem.batteryVoltage,
            telem.bmsValid);
    }

    update();
}

void VideoPanel::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::SmoothPixmapTransform, false);

    if (m_state.whiteNoiseEnabled.load()) {
        // Stretch noise to fill widget — scaling gives chunky static look.
        p.drawImage(rect(), m_noise);

        p.setPen(QColor(0, 0, 0, 60));
        for (int y = 0; y < height(); y += 2)
            p.drawLine(0, y, width(), y);
    } else {
        // Keep the offline HUD readable without manufacturing a camera image.
        p.fillRect(rect(), QColor(18, 18, 18));
    }

    // "NO SIGNAL" badge
    QFont f = font();
    f.setPointSize(qRound(f.pointSize() * 1.5) + 2);
    f.setBold(true);
    p.setFont(f);
    QFontMetrics fm(f);
    QString msg = "NO SIGNAL";
    QRect tr = fm.boundingRect(msg).adjusted(-12, -6, 12, 6);
    tr.moveCenter(rect().center());
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(0, 0, 0, 140));
    p.drawRect(tr);
    QColor noSignalColor = Theme::accent;
    noSignalColor.setAlpha(210);
    p.setPen(noSignalColor);
    p.drawText(tr, Qt::AlignCenter, msg);
}

void VideoPanel::resizeEvent(QResizeEvent* e) {
    QWidget::resizeEvent(e);
    repositionWheels();
    repositionCompass();
    repositionDashboard();
    repositionHud();
}

void VideoPanel::wheelEvent(QWheelEvent* e) {
    // Mouse wheel adjusts cruise speed the same way the keyboard's Up/Down
    // does (see InputManager::poll's cruiseAdjust handling) -- 5% per notch,
    // regardless of whether cruise is currently engaged.
    const int steps = e->angleDelta().y() / 120;
    if (steps != 0) {
        std::lock_guard<std::mutex> lk(m_state.registryMutex);
        auto& ctrl = m_state.registry.get<ControlState>(m_state.ugv);
        ctrl.cruiseSpeed = std::clamp(ctrl.cruiseSpeed + steps * 0.05f, 0.0f, 1.0f);
    }
    e->accept();
}

void VideoPanel::repositionWheels() {
    if (!m_wheels) return;
    constexpr int margin = 8;
    m_wheels->move(margin, height() - kDashboardH - m_wheels->height() - margin);
    m_wheels->raise();
}

void VideoPanel::repositionCompass() {
    if (!m_compass) return;
    m_compass->setGeometry(0, 0, width(), 44);
    m_compass->raise();
}

void VideoPanel::repositionDashboard() {
    if (!m_dashboard) return;
    // Overshoot the bottom edge on purpose (see kBottomOverbleed) instead of
    // sizing exactly to height() -- at fractional HiDPI scale factors, the
    // two widgets' backing stores can round independently and leave a 1px
    // seam of VideoPanel's own painted content peeking through below the bar.
    m_dashboard->setGeometry(0, height() - kDashboardH, width(),
                             kDashboardH + kBottomOverbleed);
    m_dashboard->raise();
}

void VideoPanel::repositionHud() {
    if (!m_hud) return;
    m_hud->move((width() - m_hud->width()) / 2, (height() - m_hud->height()) / 2);
    m_hud->raise();
}
