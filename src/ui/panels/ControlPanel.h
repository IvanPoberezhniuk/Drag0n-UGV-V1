#pragma once
#include "ui/widgets/ToggleSwitch.h"
#include "ui/IPanel.h"
#include "core/AppState.h"

class QPushButton;
class QLabel;
class QRadioButton;

class ControlPanel : public IPanel {
public:
    explicit ControlPanel(AppState& state, QWidget* parent = nullptr);
    void refresh() override;

private:
    AppState& m_state;

    QPushButton*  m_armBtn         = nullptr;
    QPushButton*  m_estopBtn       = nullptr;
    QPushButton*  m_clearFaultBtn  = nullptr;
    QLabel*       m_latchLabel     = nullptr;
    QRadioButton* m_mode1          = nullptr;
    QRadioButton* m_mode2          = nullptr;
    QRadioButton* m_mode3          = nullptr;
    ToggleSwitch* m_lightsSwitch   = nullptr;
};
