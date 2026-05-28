#include "ui/widgets/ToggleSwitch.h"
#include <QPainter>

static constexpr int kTrackW = 40;
static constexpr int kTrackH = 20;
static constexpr int kThumb  = 14;

ToggleSwitch::ToggleSwitch(const QString& label, QWidget* parent)
    : QAbstractButton(parent), m_label(label)
{
    setCheckable(true);
    setCursor(Qt::PointingHandCursor);
}

QSize ToggleSwitch::sizeHint() const {
    QFontMetrics fm(font());
    int textW = m_label.isEmpty() ? 0 : fm.horizontalAdvance(m_label) + 6;
    return { kTrackW + textW, kTrackH + 6 };
}

void ToggleSwitch::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    bool on = isChecked();
    int  r  = kTrackH / 2;

    QColor track = on ? QColor(30, 160, 30) : QColor(80, 80, 80);
    p.setPen(Qt::NoPen);
    p.setBrush(track);
    p.drawRoundedRect(QRectF(0, (height() - kTrackH) / 2.0, kTrackW, kTrackH), r, r);

    int thumbX = on ? kTrackW - kThumb - 3 : 3;
    int thumbY = (height() - kThumb) / 2;
    p.setBrush(QColor(220, 220, 220));
    p.drawEllipse(thumbX, thumbY, kThumb, kThumb);

    if (!m_label.isEmpty()) {
        p.setPen(palette().color(QPalette::WindowText));
        p.drawText(kTrackW + 6, 0, width() - kTrackW - 6, height(),
                   Qt::AlignVCenter | Qt::AlignLeft, m_label);
    }
}
