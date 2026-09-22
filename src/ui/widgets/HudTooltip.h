#pragma once
#include <QLabel>

// A minimal replacement for QToolTip::showText/hideText. Qt's own QTipLabel
// gets an OS drop shadow on Windows that proved unremovable in practice
// (the documented Qt::NoDropShadowWindowHint workaround, applied via an
// event filter on the already-created singleton, didn't take), so this
// widget is created with that flag from its very first show instead of
// having it retrofitted onto Qt's internal instance.
class HudTooltip : public QLabel {
    Q_OBJECT
public:
    static void showText(const QPoint& globalPos, const QString& html);
    static void hideText();

private:
    HudTooltip();
    static HudTooltip* instance();
};
