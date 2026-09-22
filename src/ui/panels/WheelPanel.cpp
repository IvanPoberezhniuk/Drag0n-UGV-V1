#include "ui/panels/WheelPanel.h"
#include "core/ControlState.h"
#include "core/SafetyState.h"
#include "core/StateSnapshot.h"
#include "ui/Theme.h"
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

// Amber-family palette throughout (matches the rest of the app's amber HUD
// language) -- state is communicated by brightness, not by hue. Red is
// reserved purely for the safety/estop meaning, applied below as an outline
// accent on top of a wheel's amber fill rather than as a fill of its own.
static const QColor kBodyFill        {  38,  30,   8 };
static const QColor kBodyBorder      = Theme::textMuted;
static const QColor kAxleLine        {  90,  70,  25 };
static const QColor kLightBarOn      { 255, 240, 185 };  // headlight warm white -- not a state color
static const QColor kLightBarOffFill {  50,  42,  22 };
static const QColor kLightBarOffEdge {  95,  78,  35 };

// Wheel fill/edge by engagement + power; brightness rises disabled -> idle ->
// spinning. A wheel that's engaged but can't spin for a safety reason
// (unarmed/estopped) instead gets a diagonal amber/grey hazard-stripe fill
// (see hazardStripeBrush below), replacing its normal fill entirely.
static const QColor kWheelDisabledFill{  30,  26,  14 };
static const QColor kWheelDisabledEdge{  55,  47,  25 };
static const QColor kWheelIdleFill    {  90,  68,  10 };
static const QColor kWheelIdleEdge    = Theme::textDim;
static const QColor kWheelSpinningFill= Theme::accent;
static const QColor kWheelSpinningEdge{ 255, 214, 120 };
static const QColor kWheelHazardAmber = Theme::accent;
static const QColor kWheelHazardGrey  {  45,  45,  45 };

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

    // Amber HUD container while retaining a dark translucent interior.
    QColor containerFill = Theme::accent;
    containerFill.setAlpha(22);
    p.setPen(Qt::NoPen);
    p.setBrush(containerFill);
    p.drawRect(QRectF(0, 0, kBaseW, kBaseH));

    auto [ctrl, safety] = snapshot<ControlState, SafetyState>(m_state);
    bool  armed     = ctrl.armed;
    bool  braked    = ctrl.estop || safety.estopLatched;
    bool      lightsOn  = ctrl.lightsOn;
    float     throttle  = ctrl.throttle;
    float     steering  = ctrl.steering;
    DriveMode driveMode = ctrl.driveMode;

    float leftPow  = throttle - steering * 0.5f;
    float rightPow = throttle + steering * 0.5f;
    bool  leftOn   = armed && std::abs(leftPow)  > 0.05f;
    bool  rightOn  = armed && std::abs(rightPow) > 0.05f;

    // row 0 = front, row 1 = middle, row 2 = rear
    // 2WD: rear only | 4WD: middle+rear | 6WD: all
    auto rowEngaged = [&](int row) -> bool {
        switch (driveMode) {
            case DriveMode::TwoWD:  return row == 2;
            case DriveMode::FourWD: return row >= 1;
            default:                return true; // SixWD
        }
    };

    int bodyX = kPad + kWW + kGX;
    int bodyY = kPad;

    // UGV body
    p.setPen(QPen(kBodyBorder, 1));
    p.setBrush(kBodyFill);
    p.drawRoundedRect(QRectF(bodyX, bodyY, kBW, kBodyH), 5, 5);

    // Light bar (narrow strip at front — always visible)
    {
        if (lightsOn) {
            p.setPen(Qt::NoPen);
            p.setBrush(kLightBarOn);
        } else {
            p.setPen(QPen(kLightBarOffEdge, 1));
            p.setBrush(kLightBarOffFill);
        }
        p.drawRect(QRectF(bodyX + 2, bodyY, kBW - 4, 7));
    }

    // Wheels — amber brightness by engagement/power (disabled -> idle ->
    // spinning); an engaged wheel that can't spin right now for a safety
    // reason (unarmed/estopped) gets an amber/grey diagonal hazard-stripe
    // fill instead, so it reads as "caution" without breaking the amber
    // palette with a solid red block.
    for (int i = 0; i < 3; ++i) {
        int wy      = kPad + i * (kWH + kWGap);
        int axleY   = wy + kWH / 2;
        bool engaged = rowEngaged(i);
        bool brakeIndicator = engaged && (!armed || braked);

        auto wheelColors = [&](bool sideOn) -> std::pair<QColor, QColor> {
            if (!engaged)
                return { kWheelDisabledFill, kWheelDisabledEdge };
            if (sideOn)
                return { kWheelSpinningFill, kWheelSpinningEdge };
            return     { kWheelIdleFill,     kWheelIdleEdge     };
        };

        auto drawWheel = [&](const QRectF& r, bool sideOn) {
            if (brakeIndicator) {
                p.setBackgroundMode(Qt::OpaqueMode);
                p.setBackground(QBrush(kWheelHazardGrey));
                p.setBrush(QBrush(kWheelHazardAmber, Qt::BDiagPattern));
                p.setPen(QPen(kWheelHazardAmber, 1));
                p.drawRoundedRect(r, 3, 3);
                p.setBackgroundMode(Qt::TransparentMode);
                return;
            }
            auto [fill, border] = wheelColors(sideOn);
            p.setPen(QPen(border, 1));
            p.setBrush(fill);
            p.drawRoundedRect(r, 3, 3);
        };

        // Left wheel
        p.setPen(QPen(kAxleLine, 1));
        p.drawLine(kPad + kWW, axleY, bodyX, axleY);
        drawWheel(QRectF(kPad, wy, kWW, kWH), leftOn);

        // Right wheel
        int rx = bodyX + kBW + kGX;
        p.setPen(QPen(kAxleLine, 1));
        p.drawLine(bodyX + kBW, axleY, rx, axleY);
        drawWheel(QRectF(rx, wy, kWW, kWH), rightOn);
    }
}
