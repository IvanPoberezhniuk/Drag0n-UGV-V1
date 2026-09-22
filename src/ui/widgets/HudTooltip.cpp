#include "ui/widgets/HudTooltip.h"
#include "ui/Theme.h"
#include <QGuiApplication>
#include <QScreen>

HudTooltip::HudTooltip()
    : QLabel(nullptr, Qt::ToolTip | Qt::FramelessWindowHint | Qt::NoDropShadowWindowHint)
{
    setAttribute(Qt::WA_TransparentForMouseEvents);
    setAttribute(Qt::WA_ShowWithoutActivating);
    setTextFormat(Qt::RichText);
    setMargin(0);
    setStyleSheet(QString(
        "background-color: %1; color: %2; border: 1px solid %3; padding: 4px 6px;")
        .arg(Theme::tooltipBg.name(), Theme::textPrimary.name(), Theme::tooltipBorder.name()));
}

HudTooltip* HudTooltip::instance() {
    static HudTooltip* inst = new HudTooltip();
    return inst;
}

void HudTooltip::showText(const QPoint& globalPos, const QString& html) {
    auto* tip = instance();
    tip->setText(html);
    tip->adjustSize();

    QPoint pos = globalPos + QPoint(16, 16); // offset so the cursor doesn't sit on top of the text
    if (auto* screen = QGuiApplication::screenAt(globalPos)) {
        const QRect avail = screen->availableGeometry();
        if (pos.x() + tip->width() > avail.right())   pos.setX(avail.right() - tip->width());
        if (pos.y() + tip->height() > avail.bottom()) pos.setY(avail.bottom() - tip->height());
    }
    tip->move(pos);
    tip->show();
}

void HudTooltip::hideText() {
    instance()->hide();
}
