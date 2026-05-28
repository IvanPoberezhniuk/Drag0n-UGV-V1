#pragma once
#include "core/AppState.h"
#include <QWidget>

class WheelPanel : public QWidget {
    Q_OBJECT
public:
    explicit WheelPanel(AppState& state, QWidget* parent = nullptr);
    void refresh();

protected:
    void paintEvent(QPaintEvent*) override;

private:
    AppState& m_state;
    int       m_lastSizePercent = -1;
};
