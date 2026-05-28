#pragma once
#include <QAbstractButton>
#include "ui/IPanel.h"
#include "core/AppState.h"

class QProgressBar;
class QPushButton;
class QLabel;
class QRadioButton;

class ToggleSwitch : public QAbstractButton {
public:
    explicit ToggleSwitch(const QString& label, QWidget* parent = nullptr);
    QSize sizeHint() const override;
protected:
    void paintEvent(QPaintEvent*) override;
private:
    QString m_label;
};

class ControlPanel : public IPanel {
public:
    explicit ControlPanel(AppState& state, QWidget* parent = nullptr);
    void refresh() override;

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
    ToggleSwitch* m_lightsSwitch   = nullptr;
};
