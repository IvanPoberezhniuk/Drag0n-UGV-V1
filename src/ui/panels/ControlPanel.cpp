#include "ui/panels/ControlPanel.h"
#include "core/ControlState.h"
#include "core/SafetyState.h"
#include "core/StateSnapshot.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QProgressBar>
#include <QPushButton>
#include <QLabel>
#include <QRadioButton>
#include <QButtonGroup>
#include <spdlog/spdlog.h>
#include <mutex>

ControlPanel::ControlPanel(AppState& state, QWidget* parent)
    : IPanel(parent), m_state(state)
{
    auto* layout = new QVBoxLayout(this);

    static constexpr int kBarLabelWidth = 60;
    auto makeBar = [this](const QString& labelText, QProgressBar*& bar, QHBoxLayout* row) {
        auto* label = new QLabel(labelText, this);
        label->setFixedWidth(kBarLabelWidth);
        row->addWidget(label);
        bar = new QProgressBar(this);
        bar->setRange(-100, 100);
        bar->setValue(0);
        bar->setFormat("%v%");
        bar->setTextVisible(true);
        row->addWidget(bar, 1);
    };

    auto* thrRow = new QHBoxLayout;
    makeBar("Throttle", m_throttleBar, thrRow);
    layout->addLayout(thrRow);

    auto* strRow = new QHBoxLayout;
    makeBar("Steering", m_steeringBar, strRow);
    layout->addLayout(strRow);

    // Arm + ESTOP
    auto* btnRow = new QHBoxLayout;
    m_armBtn = new QPushButton("DISARMED", this);
    m_armBtn->setMinimumHeight(30);
    m_estopBtn = new QPushButton("ESTOP", this);
    m_estopBtn->setMinimumHeight(30);
    // Style updated in refresh() based on active estop state
    btnRow->addWidget(m_armBtn);
    btnRow->addWidget(m_estopBtn);
    layout->addLayout(btnRow);

    m_cruiseLabel = new QLabel("Cruise: OFF", this);
    layout->addWidget(m_cruiseLabel);

    m_latchLabel = new QLabel("LATCHED — re-arm to clear", this);
    m_latchLabel->setStyleSheet("color: #ff6600;");
    m_latchLabel->hide();
    layout->addWidget(m_latchLabel);

    // Drive mode
    auto* modeRow = new QHBoxLayout;
    modeRow->addWidget(new QLabel("Drive mode:", this));
    m_mode1 = new QRadioButton("2WD", this);
    m_mode2 = new QRadioButton("4WD", this);
    m_mode3 = new QRadioButton("6WD", this);
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
    m_lightsSwitch = new ToggleSwitch("Lights", this);
    layout->addWidget(m_lightsSwitch);
    layout->addStretch();

    connect(m_armBtn, &QPushButton::clicked, this, [this]() {
        std::lock_guard<std::mutex> lk(m_state.registryMutex);
        auto& ctrl   = m_state.registry.get<ControlState>(m_state.ugv);
        auto& safety = m_state.registry.get<SafetyState>(m_state.ugv);
        ctrl.armed = !ctrl.armed;
        if (ctrl.armed) {
            ctrl.estop          = false;
            safety.estopLatched = false;
            spdlog::info("UI: ARMED");
        } else {
            spdlog::info("UI: DISARMED");
        }
    });

    connect(m_estopBtn, &QPushButton::clicked, this, [this]() {
        std::lock_guard<std::mutex> lk(m_state.registryMutex);
        auto& ctrl   = m_state.registry.get<ControlState>(m_state.ugv);
        auto& safety = m_state.registry.get<SafetyState>(m_state.ugv);
        ctrl.estop          = true;
        ctrl.armed          = false;
        safety.estopLatched = true;
        spdlog::warn("UI: EMERGENCY STOP");
    });

    connect(modeGroup, &QButtonGroup::idClicked, this, [this](int id) {
        std::lock_guard<std::mutex> lk(m_state.registryMutex);
        m_state.registry.get<ControlState>(m_state.ugv).driveMode = id;
    });

    connect(m_lightsSwitch, &QAbstractButton::toggled, this, [this](bool checked) {
        std::lock_guard<std::mutex> lk(m_state.registryMutex);
        m_state.registry.get<ControlState>(m_state.ugv).lightsOn = checked;
    });
}

void ControlPanel::refresh() {
    auto [ctrl, safety] = snapshot<ControlState, SafetyState>(m_state);

    m_throttleBar->setValue(static_cast<int>(ctrl.throttle * 100));
    m_steeringBar->setValue(static_cast<int>(ctrl.steering * 100));

    if (ctrl.armed) {
        m_armBtn->setText("ARMED");
        m_armBtn->setStyleSheet("QPushButton { background-color: #006600; color: white; }"
                                 "QPushButton:hover { background-color: #009900; }");
    } else {
        m_armBtn->setText("DISARMED");
        m_armBtn->setStyleSheet("");
    }

    m_latchLabel->setVisible(safety.estopLatched);

    bool estopActive = ctrl.estop || safety.estopLatched;
    if (estopActive) {
        m_estopBtn->setStyleSheet("QPushButton { background-color: #cc0000; color: white; }"
                                  "QPushButton:hover { background-color: #ff2222; }");
    } else {
        m_estopBtn->setStyleSheet("QPushButton { background-color: #4a1a1a; color: #aa6666; }"
                                  "QPushButton:hover { background-color: #6a2020; color: #dd8888; }");
    }

    if      (ctrl.driveMode == 1) m_mode1->setChecked(true);
    else if (ctrl.driveMode == 2) m_mode2->setChecked(true);
    else if (ctrl.driveMode == 3) m_mode3->setChecked(true);

    m_lightsSwitch->blockSignals(true);
    m_lightsSwitch->setChecked(ctrl.lightsOn);
    m_lightsSwitch->blockSignals(false);

    if (ctrl.cruiseEnabled) {
        m_cruiseLabel->setText(QString("Cruise: ON (%1%)").arg(static_cast<int>(ctrl.cruiseSpeed * 100)));
        m_cruiseLabel->setStyleSheet("color: #33aaff; font-weight: bold;");
    } else {
        m_cruiseLabel->setText(QString("Cruise: OFF (%1%)").arg(static_cast<int>(ctrl.cruiseSpeed * 100)));
        m_cruiseLabel->setStyleSheet("color: #888888;");
    }
}
