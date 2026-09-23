#include "ui/panels/ControlPanel.h"
#include "core/ControlState.h"
#include "core/SafetyState.h"
#include "core/StateSnapshot.h"
#include "core/ChronoTypes.h"
#include "ui/Theme.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QLabel>
#include <QRadioButton>
#include <QButtonGroup>
#include <spdlog/spdlog.h>
#include <mutex>

static const QColor kArmedBg           {   0, 102,   0 };
static const QColor kArmedBgHover      {   0, 153,   0 };
static const QColor kEstopActiveBg     { 204,   0,   0 };
static const QColor kEstopActiveBgHover{ 255,  34,  34 };
static const QColor kEstopIdleBg       {  74,  26,  26 };
static const QColor kEstopIdleText     { 170, 102, 102 };
static const QColor kEstopIdleBgHover  { 106,  32,  32 };
static const QColor kEstopIdleHoverText{ 221, 136, 136 };

ControlPanel::ControlPanel(AppState& state, QWidget* parent)
    : IPanel(parent), m_state(state)
{
    auto* layout = new QVBoxLayout(this);

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

    m_latchLabel = new QLabel("LATCHED — re-arm to clear", this);
    m_latchLabel->setStyleSheet(Theme::colorSS(Theme::cautionOrange));
    m_latchLabel->hide();
    layout->addWidget(m_latchLabel);

    // STM32 motor-node FAULT (e.g. after a command-link dropout) latches
    // until this is sent -- clearing it never re-arms by itself, matching
    // safety.c's two-step design.
    m_clearFaultBtn = new QPushButton("Clear Motor Fault", this);
    m_clearFaultBtn->setMinimumHeight(26);
    layout->addWidget(m_clearFaultBtn);

    // Drive mode
    auto* modeRow = new QHBoxLayout;
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

    connect(m_clearFaultBtn, &QPushButton::clicked, this, [this]() {
        std::lock_guard<std::mutex> lk(m_state.registryMutex);
        auto& ctrl = m_state.registry.get<ControlState>(m_state.ugv);
        ctrl.clearFault    = true;
        ctrl.clearFaultSetAt = Clock::now();
        spdlog::info("UI: CLEAR FAULT requested");
    });

    connect(modeGroup, &QButtonGroup::idClicked, this, [this](int id) {
        std::lock_guard<std::mutex> lk(m_state.registryMutex);
        m_state.registry.get<ControlState>(m_state.ugv).driveMode = static_cast<DriveMode>(id);
    });

    connect(m_lightsSwitch, &QAbstractButton::toggled, this, [this](bool checked) {
        std::lock_guard<std::mutex> lk(m_state.registryMutex);
        m_state.registry.get<ControlState>(m_state.ugv).lightsOn = checked;
    });
}

void ControlPanel::refresh() {
    auto [ctrl, safety] = snapshot<ControlState, SafetyState>(m_state);

    if (ctrl.armed) {
        m_armBtn->setText("ARMED");
        m_armBtn->setStyleSheet(Theme::pushButtonSS(kArmedBg, kArmedBgHover));
    } else {
        m_armBtn->setText("DISARMED");
        m_armBtn->setStyleSheet("");
    }

    m_latchLabel->setVisible(safety.estopLatched);

    bool estopActive = ctrl.estop || safety.estopLatched;
    if (estopActive) {
        m_estopBtn->setStyleSheet(Theme::pushButtonSS(kEstopActiveBg, kEstopActiveBgHover));
    } else {
        m_estopBtn->setStyleSheet(Theme::pushButtonSS(kEstopIdleBg, kEstopIdleBgHover,
                                                       kEstopIdleText, kEstopIdleHoverText));
    }

    switch (ctrl.driveMode) {
        case DriveMode::TwoWD:  m_mode1->setChecked(true); break;
        case DriveMode::FourWD: m_mode2->setChecked(true); break;
        case DriveMode::SixWD:  m_mode3->setChecked(true); break;
    }

    m_lightsSwitch->blockSignals(true);
    m_lightsSwitch->setChecked(ctrl.lightsOn);
    m_lightsSwitch->blockSignals(false);
}
