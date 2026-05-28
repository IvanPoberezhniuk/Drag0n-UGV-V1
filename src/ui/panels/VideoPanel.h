#pragma once
#include "core/AppState.h"
#include <QWidget>

class WheelPanel;

class VideoPanel : public QWidget {
    Q_OBJECT
public:
    explicit VideoPanel(AppState& state, QWidget* parent = nullptr);
    void refresh();

protected:
    void paintEvent(QPaintEvent*) override;
    void resizeEvent(QResizeEvent*) override;

private:
    AppState&   m_state;
    WheelPanel* m_wheels = nullptr;

    void repositionWheels();
};
