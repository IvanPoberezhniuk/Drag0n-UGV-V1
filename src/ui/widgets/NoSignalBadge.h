#pragma once
#include <QWidget>

// "NO SIGNAL" badge shown centered over VideoPanel when the camera feed
// isn't streaming. Deliberately a real sibling QWidget of HudCrosshair
// rather than something VideoPanel paints inline in its own paintEvent: a
// child widget always paints on top of its parent's own paintEvent content
// regardless of raise() (raise()/lower() only reorder among siblings, they
// can never bring a parent's own drawing above a child), so drawing this
// badge as part of VideoPanel::paintEvent could never out-rank
// HudCrosshair's z-order no matter what. As a sibling widget, VideoPanel
// can raise() it above m_hud on demand (see VideoPanel::repositionNoSignalBadge()).
class NoSignalBadge : public QWidget {
    Q_OBJECT
public:
    explicit NoSignalBadge(QWidget* parent = nullptr);

    // Plain static text -- no animation/timer here regardless of what's
    // passed (e.g. "Loading..." while connecting is deliberately just as
    // static as "NO SIGNAL"; that's a different concern from the camera
    // dashboard icon's pulsing blink, which lives entirely in DashboardBar).
    void setText(const QString& text);

    QSize sizeHint() const override;

protected:
    void paintEvent(QPaintEvent*) override;

private:
    QString m_text = "NO SIGNAL";
};
