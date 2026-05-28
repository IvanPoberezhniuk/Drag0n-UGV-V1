#pragma once
#include <QWidget>
#include "core/AppState.h"

class QProgressBar;
class QPushButton;
class QLabel;
class QRadioButton;
class QCheckBox;

class ControlPanel : public QWidget {
public:
    explicit ControlPanel(AppState& state, QWidget* parent = nullptr);
    void refresh();

private:
    AppState& m_state;

    QProgressBar* m_throttleBar    = nullptr;
    QLabel*       m_throttleLabel  = nullptr;
    QProgressBar* m_steeringBar    = nullptr;
    QLabel*       m_steeringLabel  = nullptr;
    QPushButton*  m_armBtn         = nullptr;
    QPushButton*  m_estopBtn       = nullptr;
    QLabel*       m_latchLabel     = nullptr;
    QRadioButton* m_mode1          = nullptr;
    QRadioButton* m_mode2          = nullptr;
    QRadioButton* m_mode3          = nullptr;
    QCheckBox*    m_lightsCheck    = nullptr;
};
