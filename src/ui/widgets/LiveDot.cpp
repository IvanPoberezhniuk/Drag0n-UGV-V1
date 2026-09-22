#include "ui/widgets/LiveDot.h"
#include "ui/Theme.h"
#include <QPainter>

static constexpr int kBlinkPeriodMs = 1600;

LiveDot::LiveDot(QWidget* parent) : QWidget(parent) {
    setFixedSize(sizeHint());
    m_timer.setInterval(Theme::kUiRefreshMs); // only runs while active
    connect(&m_timer, &QTimer::timeout, this, [this]() { update(); });
}

QSize LiveDot::sizeHint() const { return {10, 12}; }

void LiveDot::setActive(bool active) {
    if (active == m_active) return;
    m_active = active;
    if (active) {
        m_clock.start();
        m_timer.start();
    } else {
        m_timer.stop();
    }
    update();
}

void LiveDot::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.setPen(Qt::NoPen);

    QColor color;
    if (m_active) {
        const qint64 elapsed = m_clock.isValid() ? m_clock.elapsed() : 0;
        const double ease = Theme::pulsePhase(elapsed, kBlinkPeriodMs);
        color = Theme::accent;
        color.setAlphaF(0.35 + 0.65 * ease);
    } else {
        color = Theme::textMuted;
        color.setAlpha(160);
    }

    p.setBrush(color);
    // Keep the antialiased edge inside the widget and offset the circle
    // slightly downward for optical alignment with adjacent text glyphs.
    p.drawEllipse(QRectF(1.0, 3.0, 8.0, 8.0));
}
