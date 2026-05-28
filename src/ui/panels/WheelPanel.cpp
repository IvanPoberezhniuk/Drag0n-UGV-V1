#include "ui/panels/WheelPanel.h"
#include "core/ControlState.h"
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

    bool  armed    = false;
    float throttle = 0.0f;
    float steering = 0.0f;
    {
        std::lock_guard<std::mutex> lk(m_state.registryMutex);
        if (m_state.ugv != entt::null &&
            m_state.registry.all_of<ControlState>(m_state.ugv))
        {
            auto& ctrl = m_state.registry.get<ControlState>(m_state.ugv);
            armed    = ctrl.armed;
            throttle = ctrl.throttle;
            steering = ctrl.steering;
        }
    }

    float leftPow  = throttle - steering * 0.5f;
    float rightPow = throttle + steering * 0.5f;
    bool  leftOn   = armed && std::abs(leftPow)  > 0.05f;
    bool  rightOn  = armed && std::abs(rightPow) > 0.05f;

    int bodyX = kPad + kWW + kGX;
    int bodyY = kPad;

    // UGV body
    p.setPen(QPen(QColor(110, 110, 110), 1));
    p.setBrush(QColor(42, 44, 48));
    p.drawRoundedRect(QRectF(bodyX, bodyY, kBW, kBodyH), 5, 5);

    // Front direction arrow
    {
        int cx = bodyX + kBW / 2;
        QPolygonF arrow;
        arrow << QPointF(cx - 6, bodyY + 12)
              << QPointF(cx + 6, bodyY + 12)
              << QPointF(cx,     bodyY + 4);
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(170, 170, 170));
        p.drawPolygon(arrow);
    }

    // Wheels
    for (int i = 0; i < 3; ++i) {
        int wy    = kPad + i * (kWH + kWGap);
        int axleY = wy + kWH / 2;

        // Left wheel
        QRectF lwRect(kPad, wy, kWW, kWH);
        QColor lFill   = leftOn  ? QColor(50, 210, 50)  : QColor(55, 55, 55);
        QColor lBorder = leftOn  ? QColor(100, 255, 100) : QColor(88, 88, 88);
        p.setPen(QPen(QColor(70, 70, 70), 1));
        p.drawLine(kPad + kWW, axleY, bodyX, axleY);
        p.setPen(QPen(lBorder, 1));
        p.setBrush(lFill);
        p.drawRoundedRect(lwRect, 3, 3);

        // Right wheel
        int rx = bodyX + kBW + kGX;
        QRectF rwRect(rx, wy, kWW, kWH);
        QColor rFill   = rightOn ? QColor(50, 210, 50)  : QColor(55, 55, 55);
        QColor rBorder = rightOn ? QColor(100, 255, 100) : QColor(88, 88, 88);
        p.setPen(QPen(QColor(70, 70, 70), 1));
        p.drawLine(bodyX + kBW, axleY, rx, axleY);
        p.setPen(QPen(rBorder, 1));
        p.setBrush(rFill);
        p.drawRoundedRect(rwRect, 3, 3);
    }
}
