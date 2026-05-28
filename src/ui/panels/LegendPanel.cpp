#include "ui/panels/LegendPanel.h"
#include <QPainter>
#include <QFont>

static const QColor kKeyBg    {70,  70,  70};
static const QColor kKeyBorder{110, 110, 110};
static const QColor kKeyText  {230, 230, 230};
static const QColor kDimText  {160, 160, 160};
static const QColor kGreen    {0,   200, 80};

LegendPanel::LegendPanel(QWidget* parent) : QWidget(parent) {
    setMinimumWidth(300);
}

QSize LegendPanel::sizeHint() const { return {320, 580}; }

// ── helpers ────────────────────────────────────────────────────────────────

void LegendPanel::drawKey(QPainter& p, QRectF r, const QString& label) {
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(30, 30, 30));
    p.drawRoundedRect(r.adjusted(0, 2, 0, 2), 5, 5);

    QLinearGradient grad(r.topLeft(), r.bottomLeft());
    grad.setColorAt(0, QColor(85, 85, 85));
    grad.setColorAt(1, kKeyBg);
    p.setBrush(grad);
    p.setPen(QPen(kKeyBorder, 1));
    p.drawRoundedRect(r, 5, 5);

    QFont f("Segoe UI", 8, QFont::Bold);
    p.setFont(f);
    p.setPen(kKeyText);
    p.drawText(r, Qt::AlignCenter, label);
}

void LegendPanel::drawCircularKey(QPainter& p, int x, int y, int size,
                                   const QString& label, QColor fill) {
    QPointF c(x + size / 2.0, y + size / 2.0);
    int r = size / 2 - 1;

    // Shadow
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(25, 25, 25));
    p.drawEllipse(c + QPointF(0, 2), r, r);

    // Face
    QRadialGradient grad(c - QPointF(r * 0.2, r * 0.3), r * 1.5);
    grad.setColorAt(0, fill.lighter(140));
    grad.setColorAt(1, fill.darker(130));
    p.setBrush(grad);
    p.setPen(QPen(fill.lighter(160), 1));
    p.drawEllipse(c, r, r);

    QFont f("Segoe UI", 9, QFont::Bold);
    p.setFont(f);
    p.setPen(Qt::white);
    p.drawText(QRectF(x, y, size, size), Qt::AlignCenter, label);
}

void LegendPanel::drawSectionTitle(QPainter& p, int x, int y, const QString& text) {
    QFont f("Segoe UI", 9, QFont::Bold);
    p.setFont(f);
    p.setPen(kGreen);
    p.drawText(x, y, text);
    p.setPen(QPen(kGreen.darker(150), 1));
    p.drawLine(x, y + 4, x + 280, y + 4);
}

// ── keyboard section ───────────────────────────────────────────────────────

void LegendPanel::drawKeyboard(QPainter& p, int x, int& y) {
    drawSectionTitle(p, x, y, "Keyboard");
    y += 18;

    const int K  = 30;
    const int G  = 3;
    const int AX = x + K * 3 + G * 2 + 14;

    //      [W]
    //   [A][S][D]
    int wX = x + K + G;
    drawKey(p, {(qreal)wX,             (qreal)y,       (qreal)K, (qreal)K}, "W");
    drawKey(p, {(qreal)x,              (qreal)(y+K+G), (qreal)K, (qreal)K}, "A");
    drawKey(p, {(qreal)wX,             (qreal)(y+K+G), (qreal)K, (qreal)K}, "S");
    drawKey(p, {(qreal)(x+(K+G)*2),    (qreal)(y+K+G), (qreal)K, (qreal)K}, "D");

    QFont af("Segoe UI", 8); p.setFont(af); p.setPen(kDimText);
    p.drawText(AX, y + K/2 + 4,          "Throttle fwd / rev");
    p.drawText(AX, y + (K+G) + K/2 + 4,  "Steer left / right");
    y += K * 2 + G + 14;

    drawKey(p, {(qreal)x, (qreal)y, 52, (qreal)K}, "Enter");
    p.setFont(af); p.setPen(kDimText);
    p.drawText(x + 56, y + K/2 + 4, "Arm / Disarm");
    y += K + G + 4;

    drawKey(p, {(qreal)x, (qreal)y, 80, (qreal)K}, "Space");
    p.setPen(QColor(255, 80, 80));
    QFont ef("Segoe UI", 8, QFont::Bold); p.setFont(ef);
    p.drawText(x + 84, y + K/2 + 4, "E-STOP");
    y += K + G + 4;

    drawKey(p, {(qreal)x, (qreal)y, (qreal)K, (qreal)K}, "L");
    p.setFont(af); p.setPen(kDimText);
    p.drawText(x + K + 8, y + K/2 + 4, "Lights toggle");
    y += K + G + 4;

    drawKey(p, {(qreal)x,           (qreal)y, (qreal)K, (qreal)K}, "1");
    drawKey(p, {(qreal)(x+K+G),     (qreal)y, (qreal)K, (qreal)K}, "2");
    drawKey(p, {(qreal)(x+(K+G)*2), (qreal)y, (qreal)K, (qreal)K}, "3");
    p.setFont(af); p.setPen(kDimText);
    p.drawText(AX, y + K/2 + 4, "Drive mode");
    y += K + 20;
}

// ── controller section ─────────────────────────────────────────────────────

void LegendPanel::drawController(QPainter& p, int x, int& y) {
    drawSectionTitle(p, x, y, "Xbox Controller");
    y += 18;

    const int K = 30;
    const int G = 3;
    const int AX = x + K + 8;

    QFont af("Segoe UI", 8);

    // LT / RT — rectangular keys
    drawKey(p, {(qreal)x, (qreal)y, 42, (qreal)K}, "LT");
    p.setFont(af); p.setPen(kDimText);
    p.drawText(x + 46, y + K/2 + 4, "Brake");
    y += K + G + 4;

    drawKey(p, {(qreal)x, (qreal)y, 42, (qreal)K}, "RT");
    p.setFont(af); p.setPen(kDimText);
    p.drawText(x + 46, y + K/2 + 4, "Throttle");
    y += K + G + 4;

    // Left stick — neutral circle
    drawCircularKey(p, x, y, K, "LS", QColor(65, 65, 65));
    p.setFont(af); p.setPen(kDimText);
    p.drawText(x + K + 8, y + K/2 + 4, "Steer left / right");
    y += K + G + 4;

    // A — arm (green)
    drawCircularKey(p, x, y, K, "A", QColor(20, 160, 50));
    p.setFont(af); p.setPen(kDimText);
    p.drawText(AX, y + K/2 + 4, "Arm / Disarm");
    y += K + G + 4;

    // B — estop (red)
    drawCircularKey(p, x, y, K, "B", QColor(190, 30, 30));
    p.setPen(QColor(255, 80, 80));
    QFont ef("Segoe UI", 8, QFont::Bold); p.setFont(ef);
    p.drawText(AX, y + K/2 + 4, "E-STOP");
    y += K + G + 4;

    // Y — lights (yellow)
    drawCircularKey(p, x, y, K, "Y", QColor(190, 150, 0));
    p.setFont(af); p.setPen(kDimText);
    p.drawText(AX, y + K/2 + 4, "Lights toggle");
    y += K + G + 4;

    // X — drive mode (blue)
    drawCircularKey(p, x, y, K, "X", QColor(20, 80, 200));
    p.setFont(af); p.setPen(kDimText);
    p.drawText(AX, y + K/2 + 4, "Drive mode cycle");
    y += K + 20;
}

// ── main paint ─────────────────────────────────────────────────────────────

void LegendPanel::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.setRenderHint(QPainter::TextAntialiasing);

    p.fillRect(rect(), QColor(38, 38, 38));

    int x = 14, y = 20;
    drawKeyboard(p, x, y);
    drawController(p, x, y);
}
