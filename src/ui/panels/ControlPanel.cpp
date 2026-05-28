#include "ui/panels/ControlPanel.h"
#include "core/ControlState.h"
#include "core/SafetyState.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QProgressBar>
#include <QPushButton>
#include <QLabel>
#include <QRadioButton>
#include <QButtonGroup>
#include <QPainter>
#include <QMouseEvent>
#include <spdlog/spdlog.h>
#include <mutex>

// ── ToggleSwitch ─────────────────────────────────────────────────────────────
static constexpr int kTrackW = 40;
static constexpr int kTrackH = 20;
static constexpr int kThumb  = 14;

ToggleSwitch::ToggleSwitch(const QString& label, QWidget* parent)
    : QAbstractButton(parent), m_label(label)
{
    setCheckable(true);
    setCursor(Qt::PointingHandCursor);
}

QSize ToggleSwitch::sizeHint() const {
    QFontMetrics fm(font());
    int textW = m_label.isEmpty() ? 0 : fm.horizontalAdvance(m_label) + 6;
    return { kTrackW + textW, kTrackH + 6 };
}

void ToggleSwitch::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    bool on = isChecked();
    int  r  = kTrackH / 2;

    // Track
    QColor track = on ? QColor(30, 160, 30) : QColor(80, 80, 80);
    p.setPen(Qt::NoPen);
    p.setBrush(track);
    p.drawRoundedRect(QRectF(0, (height() - kTrackH) / 2.0, kTrackW, kTrackH), r, r);

    // Thumb
    int thumbX = on ? kTrackW - kThumb - 3 : 3;
    int thumbY = (height() - kThumb) / 2;
    p.setBrush(QColor(220, 220, 220));
    p.drawEllipse(thumbX, thumbY, kThumb, kThumb);

    // Label
    if (!m_label.isEmpty()) {
        p.setPen(palette().color(QPalette::WindowText));
        p.drawText(kTrackW + 6, 0, width() - kTrackW - 6, height(),
                   Qt::AlignVCenter | Qt::AlignLeft, m_label);
    }
}

ControlPanel::ControlPanel(AppState& state, QWidget* parent)
    : QWidget(parent), m_state(state)
{
    auto* layout = new QVBoxLayout(this);

    auto makeBar = [this](const QString& labelText, QProgressBar*& bar, QLabel*& valLabel,
                          QHBoxLayout* row) {
        row->addWidget(new QLabel(labelText, this));
        bar = new QProgressBar(this);
        bar->setRange(-100, 100);
        bar->setValue(0);
        bar->setFormat("%v%");
        bar->setTextVisible(true);
        row->addWidget(bar, 1);
        valLabel = new QLabel("0.00", this);
        valLabel->setMinimumWidth(36);
        row->addWidget(valLabel);
    };

    auto* thrRow = new QHBoxLayout;
    makeBar("Throttle", m_throttleBar, m_throttleLabel, thrRow);
    layout->addLayout(thrRow);

    auto* strRow = new QHBoxLayout;
    makeBar("Steering", m_steeringBar, m_steeringLabel, strRow);
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
    ControlState ctrl;
    SafetyState  safety;
    {
        std::lock_guard<std::mutex> lk(m_state.registryMutex);
        ctrl   = m_state.registry.get<ControlState>(m_state.ugv);
        safety = m_state.registry.get<SafetyState>(m_state.ugv);
    }

    m_throttleBar->setValue(static_cast<int>(ctrl.throttle * 100));
    m_throttleLabel->setText(QString::number(ctrl.throttle, 'f', 2));
    m_steeringBar->setValue(static_cast<int>(ctrl.steering * 100));
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
}
