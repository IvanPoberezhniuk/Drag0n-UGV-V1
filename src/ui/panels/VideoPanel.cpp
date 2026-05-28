#include "ui/panels/VideoPanel.h"
#include "ui/panels/WheelPanel.h"
#include <QPainter>

static constexpr int kNoiseW = 480;
static constexpr int kNoiseH = 360;

VideoPanel::VideoPanel(AppState& state, QWidget* parent)
    : QWidget(parent), m_state(state)
{
    setMinimumSize(320, 240);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    m_noise = QImage(kNoiseW, kNoiseH, QImage::Format_Grayscale8);

    m_wheels = new WheelPanel(state, this);
    m_wheels->show();
}

void VideoPanel::generateNoise() {
    uchar* data  = m_noise.bits();
    int    total = kNoiseW * kNoiseH;
    // Pack 4 pixels per rng call — remap 0-255 → 85-180 (muted grey range)
    int i = 0;
    for (; i + 3 < total; i += 4) {
        uint32_t r = m_rng();
        data[i]   = 85 + ((r        & 0xFF) * 95u >> 8);
        data[i+1] = 85 + (((r >> 8) & 0xFF) * 95u >> 8);
        data[i+2] = 85 + (((r >>16) & 0xFF) * 95u >> 8);
        data[i+3] = 85 + (((r >>24) & 0xFF) * 95u >> 8);
    }
    for (; i < total; ++i)
        data[i] = 85 + ((m_rng() & 0xFF) * 95u >> 8);
}

void VideoPanel::refresh() {
    generateNoise();
    m_wheels->refresh();
    repositionWheels();
    update();
}

void VideoPanel::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::SmoothPixmapTransform, false);

    // Stretch noise to fill widget — scaling gives chunky static look
    p.drawImage(rect(), m_noise);

    // Scanlines
    p.setPen(QColor(0, 0, 0, 60));
    for (int y = 0; y < height(); y += 2)
        p.drawLine(0, y, width(), y);

    // "NO SIGNAL" badge
    QFont f = font();
    f.setPointSize(qRound(f.pointSize() * 1.5) + 2);
    f.setBold(true);
    p.setFont(f);
    QFontMetrics fm(f);
    QString msg = "NO SIGNAL";
    QRect tr = fm.boundingRect(msg).adjusted(-12, -6, 12, 6);
    tr.moveCenter(rect().center());
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(0, 0, 0, 140));
    p.drawRoundedRect(tr, 4, 4);
    p.setPen(QColor(185, 60, 60, 210));
    p.drawText(tr, Qt::AlignCenter, msg);
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
