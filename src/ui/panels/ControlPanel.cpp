#include "ui/panels/ControlPanel.h"
#include "core/ControlState.h"
#include "core/SafetyState.h"
#include "core/Events.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QSlider>
#include <QPushButton>
#include <QLabel>
#include <QRadioButton>
#include <QCheckBox>
#include <QButtonGroup>
#include <mutex>

ControlPanel::ControlPanel(AppState& state, QWidget* parent)
    : QWidget(parent), m_state(state)
{
    auto* layout = new QVBoxLayout(this);

    // Throttle row
    auto* thrRow = new QHBoxLayout;
    thrRow->addWidget(new QLabel("Throttle", this));
    m_throttleSlider = new QSlider(Qt::Horizontal, this);
    m_throttleSlider->setRange(-100, 100);
    m_throttleSlider->setValue(0);
    m_throttleSlider->setEnabled(false);
    thrRow->addWidget(m_throttleSlider, 1);
    m_throttleLabel = new QLabel("0.00", this);
    m_throttleLabel->setMinimumWidth(36);
    thrRow->addWidget(m_throttleLabel);
    layout->addLayout(thrRow);

    // Steering row
    auto* strRow = new QHBoxLayout;
    strRow->addWidget(new QLabel("Steering", this));
    m_steeringSlider = new QSlider(Qt::Horizontal, this);
    m_steeringSlider->setRange(-100, 100);
    m_steeringSlider->setValue(0);
    m_steeringSlider->setEnabled(false);
    strRow->addWidget(m_steeringSlider, 1);
    m_steeringLabel = new QLabel("0.00", this);
    m_steeringLabel->setMinimumWidth(36);
    strRow->addWidget(m_steeringLabel);
    layout->addLayout(strRow);

    // Arm + ESTOP
    auto* btnRow = new QHBoxLayout;
    m_armBtn = new QPushButton("DISARMED", this);
    m_armBtn->setMinimumHeight(30);
    m_estopBtn = new QPushButton("ESTOP", this);
    m_estopBtn->setMinimumHeight(30);
    m_estopBtn->setStyleSheet("QPushButton { background-color: #cc0000; color: white; }"
                               "QPushButton:hover { background-color: #ff2222; }");
    btnRow->addWidget(m_armBtn);
    btnRow->addWidget(m_estopBtn);
    layout->addLayout(btnRow);

    m_latchLabel = new QLabel("LATCHED — re-arm to clear", this);
    m_latchLabel->setStyleSheet("color: #ff6600;");
    m_latchLabel->hide();
    layout->addWidget(m_latchLabel);

    // Drive mode
    auto* modeRow = new QHBoxLayout;
    modeRow->addWidget(new QLabel("Drive mode:", this));
    m_mode1 = new QRadioButton("1", this);
    m_mode2 = new QRadioButton("2", this);
    m_mode3 = new QRadioButton("3", this);
    m_mode1->setChecked(true);
    auto* modeGroup = new QButtonGroup(this);
    modeGroup->addButton(m_mode1, 1);
    modeGroup->addButton(m_mode2, 2);
    modeGroup->addButton(m_mode3, 3);
    modeRow->addWidget(m_mode1);
    modeRow->addWidget(m_mode2);
    modeRow->addWidget(m_mode3);
    modeRow->addStretch();
    layout->addLayout(modeRow);

    // Lights
    m_lightsCheck = new QCheckBox("Lights", this);
    layout->addWidget(m_lightsCheck);
    layout->addStretch();

    connect(m_armBtn, &QPushButton::clicked, this, [this]() {
        ControlState ctrl;
        {
            std::lock_guard<std::mutex> lk(m_state.registryMutex);
            ctrl = m_state.registry.get<ControlState>(m_state.ugv);
        }
        m_state.dispatcher.enqueue<ArmEvent>(ArmEvent{!ctrl.armed});
    });

    connect(m_estopBtn, &QPushButton::clicked, this, [this]() {
        m_state.dispatcher.enqueue<EstopEvent>();
    });

    connect(modeGroup, &QButtonGroup::idClicked, this, [this](int id) {
        ControlState ctrl;
        {
            std::lock_guard<std::mutex> lk(m_state.registryMutex);
            ctrl = m_state.registry.get<ControlState>(m_state.ugv);
        }
        m_state.dispatcher.enqueue<DriveInputEvent>(
            DriveInputEvent{ctrl.throttle, ctrl.steering, id, ctrl.lightsOn});
    });

    connect(m_lightsCheck, &QCheckBox::toggled, this, [this](bool checked) {
        ControlState ctrl;
        {
            std::lock_guard<std::mutex> lk(m_state.registryMutex);
            ctrl = m_state.registry.get<ControlState>(m_state.ugv);
        }
        m_state.dispatcher.enqueue<DriveInputEvent>(
            DriveInputEvent{ctrl.throttle, ctrl.steering, ctrl.driveMode, checked});
    });
}

void ControlPanel::refresh() {
    ControlState ctrl;
    SafetyState  safety;
    {
        std::lock_guard<std::mutex> lk(m_state.registryMutex);
        ctrl   = m_state.registry.get<ControlState>(m_state.ugv);
        safety = m_state.registry.get<SafetyState>(m_state.ugv);
    }

    m_throttleSlider->setValue(static_cast<int>(ctrl.throttle * 100));
    m_throttleLabel->setText(QString::number(ctrl.throttle, 'f', 2));
    m_steeringSlider->setValue(static_cast<int>(ctrl.steering * 100));
    m_steeringLabel->setText(QString::number(ctrl.steering, 'f', 2));

    if (ctrl.armed) {
        m_armBtn->setText("ARMED");
        m_armBtn->setStyleSheet("QPushButton { background-color: #006600; color: white; }"
                                 "QPushButton:hover { background-color: #009900; }");
    } else {
        m_armBtn->setText("DISARMED");
        m_armBtn->setStyleSheet("");
    }

    m_latchLabel->setVisible(safety.estopLatched);

    if      (ctrl.driveMode == 1) m_mode1->setChecked(true);
    else if (ctrl.driveMode == 2) m_mode2->setChecked(true);
    else if (ctrl.driveMode == 3) m_mode3->setChecked(true);

    m_lightsCheck->blockSignals(true);
    m_lightsCheck->setChecked(ctrl.lightsOn);
    m_lightsCheck->blockSignals(false);
}
