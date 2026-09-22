#pragma once

#include <QColor>
#include <QRect>
#include <QWidget>

class QPainter;
class QPainterPath;

// Large square targeting HUD. Only the four corner angles of the square are
// rendered. Throttle is a segmented vertical indicator inside the left edge;
// battery SOC/voltage is plain text in the top-right corner. No hover
// tooltips -- both readouts are already always-visible text.
class HudCrosshair : public QWidget {
    Q_OBJECT
public:
    explicit HudCrosshair(QWidget* parent = nullptr);

    void setThrottle(float fraction);                    // -1..1
    void setBatteryLevel(float fraction, float voltageV, bool valid);   // 0..1

protected:
    void paintEvent(QPaintEvent*) override;

private:
    void drawCrispPath(QPainter& painter, const QPainterPath& path,
                       const QColor& color, qreal width = 2.0) const;
    void drawCornerFrame(QPainter& painter, const QRectF& frame) const;
    void drawThrottle(QPainter& painter, const QRectF& frame) const;
    void drawBattery(QPainter& painter, const QRectF& frame) const;

    float m_throttle = 0.0f;
    float m_batteryLevel = 0.0f;
    float m_batteryVoltage = 0.0f;
    bool m_batteryValid = false;
};
