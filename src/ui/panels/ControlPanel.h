#pragma once
#include "ui/widgets/ToggleSwitch.h"
#include "ui/IPanel.h"
#include "core/AppState.h"

class QProgressBar;
class QPushButton;
class QLabel;
class QRadioButton;

class ControlPanel : public IPanel {
public:
    explicit ControlPanel(AppState& state, QWidget* parent = nullptr);
    void refresh() override;

private:
    AppState& m_state;

    QProgressBar* m_throttleBar    = nullptr;
    QProgressBar* m_steeringBar    = nullptr;
    QPushButton*  m_armBtn         = nullptr;
    QPushButton*  m_estopBtn       = nullptr;
    QLabel*       m_cruiseLabel    = nullptr;
    QLabel*       m_latchLabel     = nullptr;
    QRadioButton* m_mode1          = nullptr;
    QRadioButton* m_mode2          = nullptr;
    QRadioButton* m_mode3          = nullptr;
    ToggleSwitch* m_lightsSwitch   = nullptr;
};
