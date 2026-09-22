#include "ui/widgets/HoverInfoIcon.h"
#include "ui/widgets/HudTooltip.h"
#include <QCursor>
#include <QIcon>

HoverInfoIcon::HoverInfoIcon(QWidget* parent)
    : QLabel(parent)
{
    setPixmap(QIcon(":/icons/info-circle.svg").pixmap(18, 18));
    setFixedSize(24, 24);
    setAlignment(Qt::AlignCenter);
    setCursor(Qt::WhatsThisCursor);
}

void HoverInfoIcon::setTooltipHtml(const QString& html) {
    m_html = html;
}

void HoverInfoIcon::enterEvent(QEnterEvent*) {
    HudTooltip::showText(QCursor::pos(), m_html);
}

void HoverInfoIcon::leaveEvent(QEvent*) {
    HudTooltip::hideText();
}
