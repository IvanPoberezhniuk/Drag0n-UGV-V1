#pragma once
#include <QWidget>
#include <QTimer>
#include <QElapsedTimer>

// Small circular indicator that pulses ease-in-out while active, and sits
// dim/static otherwise -- a heartbeat cue for "data is actively arriving"
// distinct from a plain static connected/disconnected label.
class LiveDot : public QWidget {
    Q_OBJECT
public:
    explicit LiveDot(QWidget* parent = nullptr);

    void setActive(bool active);
    QSize sizeHint() const override;

protected:
    void paintEvent(QPaintEvent*) override;

private:
    bool m_active = false;
    QTimer m_timer;
    QElapsedTimer m_clock;
};
