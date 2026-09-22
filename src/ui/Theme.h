#pragma once
#include <QColor>
#include <QPalette>
#include <QString>
#include <cmath>

namespace Theme {

static const QColor textMuted     { 140, 105,  40 };  // faded amber
static const QColor textDim       { 189, 140,  40 };  // medium amber
static const QColor textPrimary   { 255, 191,  79 };  // bright amber (retro CRT body text)
static const QColor successGreen  {   0, 230,  51 };
static const QColor warningYellow { 255, 204,   0 };
static const QColor errorRed      { 255,  68,  68 };
static const QColor cautionOrange { 255, 140,   0 };
static const QColor accent        { 255, 176,   0 };  // amber CRT / retro HUD accent

// Shared HudTooltip chrome (see ui/widgets/HudTooltip.h).
static const QColor tooltipBg     {  38,  38,  38 };
static const QColor tooltipBorder {  58,  58,  58 };

// Translucent strip behind CompassBar/DashboardBar, so both HUD overlay bars
// share one background tint.
static const QColor hudStripBackground{ 0, 0, 0, 130 };

inline QString colorSS(QColor c) { return "color: " + c.name() + ";"; }

// QPushButton background/hover/text stylesheet, for panels that recolor a
// button per state (armed, e-stop, ...) instead of hand-writing the same
// "QPushButton { ... } QPushButton:hover { ... }" template each time.
inline QString pushButtonSS(const QColor& bg, const QColor& bgHover,
                            const QColor& text = Qt::white,
                            const QColor& hoverText = QColor()) {
    const QColor ht = hoverText.isValid() ? hoverText : text;
    return QString("QPushButton { background-color: %1; color: %2; }"
                    "QPushButton:hover { background-color: %3; color: %4; }")
        .arg(bg.name(), text.name(), bgHover.name(), ht.name());
}

// Shared UI animation-timer interval (~30fps), used by every HUD element
// that repaints on a QTimer while live (LiveDot, DashboardBar).
constexpr int kUiRefreshMs = 33;

// Ease-in-out pulse value (0..1) for a periodMs-long cycle at elapsedMs --
// shared by every HUD element that "breathes" while live (LiveDot,
// DashboardBar's camera-streaming indicator).
inline double pulsePhase(qint64 elapsedMs, int periodMs) {
    constexpr double kPi = 3.14159265358979323846;
    const double phase = std::fmod(static_cast<double>(elapsedMs), static_cast<double>(periodMs)) / periodMs;
    return 0.5 - 0.5 * std::cos(2.0 * kPi * phase);
}

inline QPalette darkPalette() {
    QPalette p;
    p.setColor(QPalette::Window,          QColor(45,  45,  45));
    p.setColor(QPalette::WindowText,      textPrimary);
    p.setColor(QPalette::Base,            QColor(30,  30,  30));
    p.setColor(QPalette::AlternateBase,   QColor(53,  53,  53));
    p.setColor(QPalette::ToolTipBase,     QColor(24, 24, 24));
    p.setColor(QPalette::ToolTipText,     textPrimary);
    p.setColor(QPalette::Text,            textPrimary);
    p.setColor(QPalette::Button,          QColor(53,  53,  53));
    p.setColor(QPalette::ButtonText,      textPrimary);
    p.setColor(QPalette::BrightText,      Qt::red);
    p.setColor(QPalette::Highlight,       accent);
    p.setColor(QPalette::HighlightedText, Qt::black);
    p.setColor(QPalette::Link,            accent);
    return p;
}

} // namespace Theme
