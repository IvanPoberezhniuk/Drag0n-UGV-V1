#include "ui/HudCrosshair.h"

#include "ui/Theme.h"

#include <QFontMetrics>
#include <QPainter>
#include <QPainterPath>
#include <algorithm>
#include <cmath>

namespace {

constexpr int kHudWidth = 960;
constexpr int kHudHeight = 560;
constexpr int kFrameInset = 6;
constexpr int kCornerLength = 64;
constexpr qreal kContentInset = 21.0;
constexpr qreal kCenterDotRadius = 3.0;

constexpr qreal kThrottleXInset = kContentInset;
constexpr qreal kThrottleTopInset = kContentInset;
constexpr qreal kThrottleBottomInset = kContentInset;
constexpr qreal kThrottleSegmentWidth = 24.0;
constexpr qreal kThrottleSegmentThickness = 3.0;
constexpr qreal kThrottleLineGap = 2.0; // gap between adjacent segment lines

constexpr qreal kBatteryRightInset = kContentInset;
constexpr qreal kBatteryTopInset = kContentInset;
constexpr int kBatteryPctFontPx = 13;   // SOC percentage text, e.g. "45%"
constexpr int kBatteryVoltFontPx = 13;  // pack voltage, to its left
constexpr qreal kBatteryVoltGap = 10.0; // gap between voltage text and percentage text

} // namespace

HudCrosshair::HudCrosshair(QWidget* parent)
    : QWidget(parent)
{
    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_NoSystemBackground);
    setFixedSize(kHudWidth, kHudHeight);
}

void HudCrosshair::setThrottle(float fraction)
{
    m_throttle = std::clamp(fraction, -1.0f, 1.0f);
    update();
}

void HudCrosshair::setBatteryLevel(float fraction, float voltageV, bool valid)
{
    m_batteryLevel = std::clamp(fraction, 0.0f, 1.0f);
    m_batteryVoltage = voltageV;
    m_batteryValid = valid;
    update();
}

void HudCrosshair::drawCrispPath(QPainter& painter,
                                 const QPainterPath& path,
                                 const QColor& color,
                                 qreal width) const
{
    painter.setBrush(Qt::NoBrush);
    painter.setPen(QPen(color, width, Qt::SolidLine,
                        Qt::SquareCap, Qt::MiterJoin));
    painter.drawPath(path);
}

void HudCrosshair::drawCornerFrame(QPainter& painter, const QRectF& frame) const
{
    QPainterPath corners;

    // Top-left.
    corners.moveTo(frame.left(), frame.top() + kCornerLength);
    corners.lineTo(frame.left(), frame.top());
    corners.lineTo(frame.left() + kCornerLength, frame.top());

    // Top-right.
    corners.moveTo(frame.right() - kCornerLength, frame.top());
    corners.lineTo(frame.right(), frame.top());
    corners.lineTo(frame.right(), frame.top() + kCornerLength);

    // Bottom-right.
    corners.moveTo(frame.right(), frame.bottom() - kCornerLength);
    corners.lineTo(frame.right(), frame.bottom());
    corners.lineTo(frame.right() - kCornerLength, frame.bottom());

    // Bottom-left.
    corners.moveTo(frame.left() + kCornerLength, frame.bottom());
    corners.lineTo(frame.left(), frame.bottom());
    corners.lineTo(frame.left(), frame.bottom() - kCornerLength);

    QColor frameColor = Theme::accent;
    frameColor.setAlpha(100); // Match inactive throttle segments.
    drawCrispPath(painter, corners, frameColor, 2.5);
}

void HudCrosshair::drawCenterDot(QPainter& painter, const QRectF& frame) const
{
    painter.setPen(Qt::NoPen);
    painter.setBrush(Theme::accent);
    painter.drawEllipse(frame.center(), kCenterDotRadius, kCenterDotRadius);
}

void HudCrosshair::drawThrottle(QPainter& painter, const QRectF& frame) const
{
    const qreal top = frame.top() + kThrottleTopInset;
    const qreal bottom = frame.bottom() - kThrottleBottomInset;
    const qreal x = frame.left() + kThrottleXInset;
    const qreal step = kThrottleSegmentThickness + kThrottleLineGap;
    int segments = static_cast<int>((bottom - top) / step) + 1;
    if (segments % 2 == 0) segments -= 1; // odd count so one segment sits exactly on center
    segments = std::max(segments, 1);
    const int center = segments / 2;
    const int activeCount = qRound(std::abs(m_throttle) * center);
    const bool forward = m_throttle > 0.02f;
    const bool reverse = m_throttle < -0.02f;

    for (int i = 0; i < segments; ++i) {
        const qreal y = top + i * step;
        const bool active = (forward && i < center && i >= center - activeCount) ||
                            (reverse && i > center && i <= center + activeCount);

        QColor color = Theme::accent;
        qreal width = kThrottleSegmentThickness;
        qreal segmentWidth = kThrottleSegmentWidth;
        if (i == center) {
            color.setAlpha(180);
            segmentWidth += 6.0;
        } else if (active) {
            color.setAlpha(240);
            width = 4.0;
        } else {
            color.setAlpha(100);
        }

        painter.setPen(QPen(color, width, Qt::SolidLine, Qt::FlatCap));
        painter.drawLine(QPointF(x, y), QPointF(x + segmentWidth, y));
    }
}

void HudCrosshair::drawBattery(QPainter& painter, const QRectF& frame) const
{
    // No BMS data at all -- draw nothing rather than a placeholder.
    if (!m_batteryValid) return;

    const QColor color = Theme::accent;
    painter.setPen(color);

    QFont pctFont = painter.font();
    pctFont.setBold(true);
    pctFont.setPixelSize(kBatteryPctFontPx);
    painter.setFont(pctFont);
    const QFontMetrics pctFm(pctFont);
    const QString pctText = QString("%1%").arg(qRound(m_batteryLevel * 100.0f));
    const qreal pctTextW = pctFm.horizontalAdvance(pctText);
    const QRectF pctRect(frame.right() - kBatteryRightInset - pctTextW,
                         frame.top() + kBatteryTopInset,
                         pctTextW, pctFm.height());
    painter.drawText(pctRect, Qt::AlignCenter, pctText);

    QFont voltFont = painter.font();
    voltFont.setBold(true);
    voltFont.setPixelSize(kBatteryVoltFontPx);
    painter.setFont(voltFont);
    const QFontMetrics voltFm(voltFont);
    const QString voltText = QString("%1V").arg(static_cast<double>(m_batteryVoltage), 0, 'f', 1);
    const qreal voltTextW = voltFm.horizontalAdvance(voltText);
    const QRectF voltRect(pctRect.left() - kBatteryVoltGap - voltTextW,
                          pctRect.center().y() - voltFm.height() / 2.0,
                          voltTextW, voltFm.height());
    painter.drawText(voltRect, Qt::AlignCenter, voltText);
}

void HudCrosshair::paintEvent(QPaintEvent*)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    const QRectF frame = QRectF(rect()).adjusted(
        kFrameInset, kFrameInset, -kFrameInset, -kFrameInset);
    drawCornerFrame(painter, frame);
    drawCenterDot(painter, frame);
    drawThrottle(painter, frame);
    drawBattery(painter, frame);
}
