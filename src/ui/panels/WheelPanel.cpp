#include "ui/panels/WheelPanel.h"
#include "core/ControlState.h"
#include "core/SafetyState.h"
#include <QPainter>
#include <cmath>

// Base dimensions at 100% (1.5× the original design)
static constexpr int kWW    = 18;   // wheel width
static constexpr int kWH    = 33;   // wheel height
static constexpr int kWGap  = 10;   // vertical gap between wheels
static constexpr int kBW    = 45;   // body width
static constexpr int kGX    = 8;    // horizontal gap body↔wheel
static constexpr int kPad   = 15;   // outer padding

static constexpr int kBodyH  = 3 * kWH + 2 * kWGap;
static constexpr int kBaseW  = kPad + kWW + kGX + kBW + kGX + kWW + kPad;
static constexpr int kBaseH  = kPad + kBodyH + kPad;

WheelPanel::WheelPanel(AppState& state, QWidget* parent)
    : QWidget(parent), m_state(state)
{
    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_NoSystemBackground);
    setFixedSize(kBaseW, kBaseH);
}

void WheelPanel::refresh() {
    int pct = m_state.wheelSizePercent.load();
    if (pct != m_lastSizePercent) {
        m_lastSizePercent = pct;
        float s = pct / 100.0f;
        setFixedSize(qRound(kBaseW * s), qRound(kBaseH * s));
    }
    update();
}

void WheelPanel::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    // Scale all drawing to fit the current widget size
    float s = width() / float(kBaseW);
    p.scale(s, s);

    // Semi-transparent background
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(0, 0, 0, 140));
    p.drawRoundedRect(QRectF(0, 0, kBaseW, kBaseH), 6, 6);

    bool  armed     = false;
    bool  braked    = false;
    bool  lightsOn  = false;
    float throttle  = 0.0f;
    float steering  = 0.0f;
    int   driveMode = 1;
    {
        std::lock_guard<std::mutex> lk(m_state.registryMutex);
        if (m_state.ugv != entt::null &&
            m_state.registry.all_of<ControlState>(m_state.ugv))
        {
            auto& ctrl   = m_state.registry.get<ControlState>(m_state.ugv);
            auto& safety = m_state.registry.get<SafetyState>(m_state.ugv);
            armed     = ctrl.armed;
            braked    = ctrl.estop || safety.estopLatched;
            lightsOn  = ctrl.lightsOn;
            throttle  = ctrl.throttle;
            steering  = ctrl.steering;
            driveMode = ctrl.driveMode;
        }
    }

    float leftPow  = throttle - steering * 0.5f;
    float rightPow = throttle + steering * 0.5f;
    bool  leftOn   = armed && std::abs(leftPow)  > 0.05f;
    bool  rightOn  = armed && std::abs(rightPow) > 0.05f;

    // row 0 = front, row 1 = middle, row 2 = rear
    // 2WD=1: rear only | 4WD=2: middle+rear | 6WD=3: all
    auto rowEngaged = [&](int row) -> bool {
        switch (driveMode) {
            case 1:  return row == 2;
            case 2:  return row >= 1;
            default: return true;
        }
    };

    int bodyX = kPad + kWW + kGX;
    int bodyY = kPad;

    // UGV body
    p.setPen(QPen(QColor(110, 110, 110), 1));
    p.setBrush(QColor(42, 44, 48));
    p.drawRoundedRect(QRectF(bodyX, bodyY, kBW, kBodyH), 5, 5);

    // Light bar (narrow strip at front — always visible)
    {
        if (lightsOn) {
            p.setPen(Qt::NoPen);
            p.setBrush(QColor(255, 240, 185));
        } else {
            p.setPen(QPen(QColor(90, 90, 90), 1));
            p.setBrush(QColor(55, 55, 55));
        }
        p.drawRect(QRectF(bodyX + 2, bodyY, kBW - 4, 7));
    }

    // Wheels — four visual states per engaged wheel:
    //   braked / unarmed                    → red
    //   spinning (armed + engaged + power)  → green
    //   engaged  (armed, no power)          → blue-grey
    //   disabled (not in drive mode)        → dark grey
    for (int i = 0; i < 3; ++i) {
        int wy      = kPad + i * (kWH + kWGap);
        int axleY   = wy + kWH / 2;
        bool engaged = rowEngaged(i);

        auto wheelColors = [&](bool sideOn) -> std::pair<QColor, QColor> {
            if (!engaged)
                return { QColor(38, 38, 38),   QColor(58,  58,  58)  };  // disabled
            if (!armed || braked)
                return { QColor(170, 28, 28),  QColor(220, 55,  55)  };  // braked / unarmed
            if (sideOn)
                return { QColor(50, 210, 50),  QColor(100, 255, 100) };  // spinning
            return     { QColor(55, 80, 110),  QColor(80,  115, 155) };  // engaged, idle
        };

        // Left wheel
        auto [lFill, lBorder] = wheelColors(leftOn);
        p.setPen(QPen(QColor(70, 70, 70), 1));
        p.drawLine(kPad + kWW, axleY, bodyX, axleY);
        p.setPen(QPen(lBorder, 1));
        p.setBrush(lFill);
        p.drawRoundedRect(QRectF(kPad, wy, kWW, kWH), 3, 3);

        // Right wheel
        int rx = bodyX + kBW + kGX;
        auto [rFill, rBorder] = wheelColors(rightOn);
        p.setPen(QPen(QColor(70, 70, 70), 1));
        p.drawLine(bodyX + kBW, axleY, rx, axleY);
        p.setPen(QPen(rBorder, 1));
        p.setBrush(rFill);
        p.drawRoundedRect(QRectF(rx, wy, kWW, kWH), 3, 3);
    }
}
