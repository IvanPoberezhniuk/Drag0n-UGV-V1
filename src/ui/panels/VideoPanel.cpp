#include "ui/panels/VideoPanel.h"
#include <QPainter>

VideoPanel::VideoPanel(QWidget* parent) : QWidget(parent) {
    setMinimumSize(320, 240);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
}

void VideoPanel::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.fillRect(rect(), Qt::black);
    p.setPen(QColor(80, 80, 80));
    p.setFont(QFont(font().family(), 14));
    p.drawText(rect(), Qt::AlignCenter, "No video feed");
}
