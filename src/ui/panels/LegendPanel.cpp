#include "ui/panels/LegendPanel.h"
#include "ui/Theme.h"
#include <QPainter>
#include <QFont>
#include <QFontMetrics>
#include <algorithm>

static const QColor kKeyBg    {70,  70,  70};
static const QColor kKeyBorder{110, 110, 110};
static const QColor kKeyText  = Theme::textPrimary;
static const QColor kDimText  = Theme::textDim;

// Xbox controller face-button/stick colors (brand-matched, not theme-shared).
static const QColor kStickGray  { 65,  65,  65};
static const QColor kButtonAGreen{ 20, 160,  50};
static const QColor kButtonBRed { 190,  30,  30};
static const QColor kButtonYYellow{190, 150,   0};
static const QColor kButtonXBlue{ 20,  80, 200};

static constexpr int kColumnGap = 28;
static constexpr int kMargin    = 14;
static constexpr int kTopMargin = 20;

LegendPanel::LegendPanel(AppState& state, QWidget* parent)
    : IPanel(parent), m_state(state) {
    setMinimumWidth(300);
}

QSize LegendPanel::sizeHint() const { return {320, 660}; }

void LegendPanel::refresh() {
    if (m_state.activeInput != m_lastInput) {
        m_lastInput = m_state.activeInput;
        update();
    }
}

// ── helpers ────────────────────────────────────────────────────────────────

void LegendPanel::drawKey(QPainter& p, QRectF r, const QString& label) const {
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(30, 30, 30));
    p.drawRoundedRect(r.adjusted(0, 2, 0, 2), 5, 5);

    QLinearGradient grad(r.topLeft(), r.bottomLeft());
    grad.setColorAt(0, QColor(85, 85, 85));
    grad.setColorAt(1, kKeyBg);
    p.setBrush(grad);
    p.setPen(QPen(kKeyBorder, 1));
    p.drawRoundedRect(r, 5, 5);

    QFont f = p.font(); f.setBold(true);
    p.setFont(f);
    p.setPen(kKeyText);
    p.drawText(r, Qt::AlignCenter, label);
}

void LegendPanel::drawCircularKey(QPainter& p, int x, int y, int size,
                                   const QString& label, QColor fill) const {
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

    QFont f = p.font(); f.setBold(true);
    p.setFont(f);
    p.setPen(Qt::white);
    p.drawText(QRectF(x, y, size, size), Qt::AlignCenter, label);
}

void LegendPanel::drawSectionTitle(QPainter& p, int x, int y, const QString& text) const {
    QFont f = p.font(); f.setBold(true);
    p.setFont(f);
    p.setPen(Theme::accent);
    p.drawText(x, y, text);
    p.setPen(QPen(Theme::accent.darker(150), 1));
    p.drawLine(x, y + 4, x + 200, y + 4);
}

// ── keyboard entries ───────────────────────────────────────────────────────

std::vector<LegendPanel::Entry> LegendPanel::buildKeyboardEntries(int K, int G, int AX) const {
    std::vector<Entry> entries;

    entries.push_back({K * 2 + G + 14, [this, K, G, AX](QPainter& p, int x, int y) {
        //      [W]
        //   [A][S][D]
        int wX = x + K + G;
        drawKey(p, {(qreal)wX,          (qreal)y,       (qreal)K, (qreal)K}, "W");
        drawKey(p, {(qreal)x,           (qreal)(y+K+G), (qreal)K, (qreal)K}, "A");
        drawKey(p, {(qreal)wX,          (qreal)(y+K+G), (qreal)K, (qreal)K}, "S");
        drawKey(p, {(qreal)(x+(K+G)*2), (qreal)(y+K+G), (qreal)K, (qreal)K}, "D");

        p.setPen(kDimText);
        p.drawText(x + AX, y + K/2 + 4,          "Throttle fwd / rev");
        p.drawText(x + AX, y + (K+G) + K/2 + 4,  "Steer left / right");
    }});

    const int kRectKeyW = 52; // "Enter" / "End"

    entries.push_back({K + G + 4, [this, K, AX, kRectKeyW](QPainter& p, int x, int y) {
        drawKey(p, {(qreal)x, (qreal)y, (qreal)kRectKeyW, (qreal)K}, "Enter");
        p.setPen(kDimText);
        p.drawText(x + AX, y + K/2 + 4, "Arm / Disarm");
    }});

    entries.push_back({K + G + 4, [this, K, AX, kRectKeyW](QPainter& p, int x, int y) {
        drawKey(p, {(qreal)x, (qreal)y, (qreal)kRectKeyW, (qreal)K}, "End");
        QFont ef = p.font(); ef.setBold(true); p.setFont(ef);
        p.setPen(Theme::errorRed);
        p.drawText(x + AX, y + K/2 + 4, "E-STOP");
    }});

    entries.push_back({K + G + 4, [this, K, AX](QPainter& p, int x, int y) {
        drawKey(p, {(qreal)x, (qreal)y, (qreal)K, (qreal)K}, "L");
        p.setPen(kDimText);
        p.drawText(x + AX, y + K/2 + 4, "Lights toggle");
    }});

    entries.push_back({K + G + 4, [this, K, G, AX](QPainter& p, int x, int y) {
        drawKey(p, {(qreal)x,           (qreal)y, (qreal)K, (qreal)K}, "1");
        drawKey(p, {(qreal)(x+K+G),     (qreal)y, (qreal)K, (qreal)K}, "2");
        drawKey(p, {(qreal)(x+(K+G)*2), (qreal)y, (qreal)K, (qreal)K}, "3");
        p.setPen(kDimText);
        p.drawText(x + AX, y + K/2 + 4, "Drive mode");
    }});

    entries.push_back({K + G + 4, [this, K, AX](QPainter& p, int x, int y) {
        drawKey(p, {(qreal)x, (qreal)y, (qreal)K, (qreal)K}, "R");
        p.setPen(kDimText);
        p.drawText(x + AX, y + K/2 + 4, "Clear motor fault");
    }});

    entries.push_back({K + G + 4, [this, K, AX](QPainter& p, int x, int y) {
        drawKey(p, {(qreal)x, (qreal)y, (qreal)K, (qreal)K}, "C");
        p.setPen(kDimText);
        p.drawText(x + AX, y + K/2 + 4, "Toggle cruise");
    }});

    entries.push_back({K + 20, [this, K, G, AX](QPainter& p, int x, int y) {
        drawKey(p, {(qreal)x,       (qreal)y, (qreal)K, (qreal)K}, QChar(0x2191)); // Up
        drawKey(p, {(qreal)(x+K+G), (qreal)y, (qreal)K, (qreal)K}, QChar(0x2193)); // Down
        p.setPen(kDimText);
        p.drawText(x + AX, y + K/2 + 4, "Cruise speed +/- 5%");
    }});

    return entries;
}

// ── controller entries ─────────────────────────────────────────────────────

std::vector<LegendPanel::Entry> LegendPanel::buildControllerEntries(int K, int G, int AX) const {
    std::vector<Entry> entries;

    entries.push_back({18, [this](QPainter& p, int x, int y) {
        drawSectionTitle(p, x, y + 14, "Xbox Controller");
    }});

    entries.push_back({K + G + 4, [this, K](QPainter& p, int x, int y) {
        drawKey(p, {(qreal)x, (qreal)y, 42, (qreal)K}, "LT");
        p.setPen(kDimText);
        p.drawText(x + 46, y + K/2 + 4, "Brake");
    }});

    entries.push_back({K + G + 4, [this, K](QPainter& p, int x, int y) {
        drawKey(p, {(qreal)x, (qreal)y, 42, (qreal)K}, "RT");
        p.setPen(kDimText);
        p.drawText(x + 46, y + K/2 + 4, "Throttle");
    }});

    entries.push_back({K + G + 4, [this, K](QPainter& p, int x, int y) {
        drawCircularKey(p, x, y, K, "LS", kStickGray);
        p.setPen(kDimText);
        p.drawText(x + K + 8, y + K/2 + 4, "Steer left / right");
    }});

    entries.push_back({K + G + 4, [this, K, AX](QPainter& p, int x, int y) {
        drawCircularKey(p, x, y, K, "A", kButtonAGreen);
        p.setPen(kDimText);
        p.drawText(x + AX, y + K/2 + 4, "Arm / Disarm");
    }});

    entries.push_back({K + G + 4, [this, K, AX](QPainter& p, int x, int y) {
        drawCircularKey(p, x, y, K, "B", kButtonBRed);
        QFont ef = p.font(); ef.setBold(true); p.setFont(ef);
        p.setPen(Theme::errorRed);
        p.drawText(x + AX, y + K/2 + 4, "E-STOP");
    }});

    entries.push_back({K + G + 4, [this, K, AX](QPainter& p, int x, int y) {
        drawCircularKey(p, x, y, K, "Y", kButtonYYellow);
        p.setPen(kDimText);
        p.drawText(x + AX, y + K/2 + 4, "Lights toggle");
    }});

    entries.push_back({K + 20, [this, K, AX](QPainter& p, int x, int y) {
        drawCircularKey(p, x, y, K, "X", kButtonXBlue);
        p.setPen(kDimText);
        p.drawText(x + AX, y + K/2 + 4, "Drive mode cycle");
    }});

    return entries;
}

// ── main paint ─────────────────────────────────────────────────────────────

void LegendPanel::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.setRenderHint(QPainter::TextAntialiasing);

    p.fillRect(rect(), Theme::tooltipBg);

    const int K = fontMetrics().height() + 12;  // key size scales with font
    const int G = 3;
    const int AX = std::max(K * 3 + G * 2, 52) + 14; // offset from an entry's own x to its description text
    const int columnWidth = AX + 190 + kColumnGap;

    const bool gamepad = m_state.activeInput == InputType::Gamepad;
    std::vector<Entry> entries = gamepad ? buildControllerEntries(K, G, AX)
                                          : buildKeyboardEntries(K, G, AX);

    const int maxY = std::max(height() - kMargin, kTopMargin);
    int x = kMargin, y = kTopMargin;
    for (const Entry& entry : entries) {
        if (y != kTopMargin && y + entry.height > maxY) {
            x += columnWidth;
            y = kTopMargin;
        }
        entry.draw(p, x, y);
        y += entry.height;
    }
}
