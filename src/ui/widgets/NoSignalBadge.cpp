#include "ui/widgets/NoSignalBadge.h"
#include "ui/Theme.h"
#include <QPainter>
#include <QFontMetrics>

namespace {
const QColor kNoSignalBadgeBg{0, 0, 0, 140};

// Same "NO SIGNAL" text styling VideoPanel used to paint inline: 1.5x point
// size + 2, bold.
QFont badgeFont(const QFont& base) {
    QFont f = base;
    f.setPointSize(qRound(f.pointSize() * 1.5) + 2);
    f.setBold(true);
    return f;
}
} // namespace

NoSignalBadge::NoSignalBadge(QWidget* parent) : QWidget(parent) {
    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_NoSystemBackground);
    // Purely a status overlay -- never intercept clicks/hover meant for
    // whatever is beneath it in VideoPanel.
    setAttribute(Qt::WA_TransparentForMouseEvents);
}

void NoSignalBadge::setText(const QString& text) {
    if (text == m_text) return;
    m_text = text;
    updateGeometry();
    update();
}

QSize NoSignalBadge::sizeHint() const {
    const QFontMetrics fm(badgeFont(font()));
    return fm.boundingRect(m_text).adjusted(-12, -6, 12, 6).size();
}

void NoSignalBadge::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setPen(Qt::NoPen);
    p.setBrush(kNoSignalBadgeBg);
    p.drawRect(rect());

    p.setFont(badgeFont(font()));
    QColor noSignalColor = Theme::accent;
    noSignalColor.setAlpha(210);
    p.setPen(noSignalColor);
    p.drawText(rect(), Qt::AlignCenter, m_text);
}
