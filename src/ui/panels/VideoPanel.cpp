#include "ui/panels/VideoPanel.h"
#include "ui/panels/WheelPanel.h"
#include <QPainter>

VideoPanel::VideoPanel(AppState& state, QWidget* parent)
    : QWidget(parent), m_state(state)
{
    setMinimumSize(320, 240);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    m_wheels = new WheelPanel(state, this);
    m_wheels->show();
}

void VideoPanel::refresh() {
    m_wheels->refresh();
    repositionWheels();
}

void VideoPanel::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.fillRect(rect(), Qt::black);
    p.setPen(QColor(80, 80, 80));
    p.setFont(font());
    p.drawText(rect(), Qt::AlignCenter, "No video feed");
}

void VideoPanel::resizeEvent(QResizeEvent* e) {
    QWidget::resizeEvent(e);
    repositionWheels();
}

void VideoPanel::repositionWheels() {
    if (!m_wheels) return;
    constexpr int margin = 8;
    m_wheels->move(margin, height() - m_wheels->height() - margin);
    m_wheels->raise();
}
