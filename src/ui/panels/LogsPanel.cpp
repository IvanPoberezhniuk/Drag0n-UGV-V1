#include "ui/panels/LogsPanel.h"
#include "core/LogBuffer.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QCheckBox>
#include <QTextEdit>
#include <QPushButton>
#include <QFont>
#include <QScrollBar>
#include <spdlog/spdlog.h>

static QString levelColor(spdlog::level::level_enum lvl) {
    switch (lvl) {
        case spdlog::level::debug:    return "#999999";
        case spdlog::level::info:     return "#e5e5e5";
        case spdlog::level::warn:     return "#ffcc00";
        case spdlog::level::err:
        case spdlog::level::critical: return "#ff4444";
        default:                      return "#e5e5e5";
    }
}

static bool shouldShow(spdlog::level::level_enum lvl,
    bool debug, bool info, bool warn, bool error)
{
    switch (lvl) {
        case spdlog::level::trace:
        case spdlog::level::debug:    return debug;
        case spdlog::level::info:     return info;
        case spdlog::level::warn:     return warn;
        case spdlog::level::err:
        case spdlog::level::critical: return error;
        default: return true;
    }
}

LogsPanel::LogsPanel(AppState& state, QWidget* parent)
    : QWidget(parent), m_state(state)
{
    auto* layout = new QVBoxLayout(this);

    auto* toolbar = new QHBoxLayout;
    m_showDebug  = new QCheckBox("DEBUG", this); m_showDebug->setChecked(true);
    m_showInfo   = new QCheckBox("INFO",  this); m_showInfo->setChecked(true);
    m_showWarn   = new QCheckBox("WARN",  this); m_showWarn->setChecked(true);
    m_showError  = new QCheckBox("ERROR", this); m_showError->setChecked(true);
    m_autoScroll = new QCheckBox("Auto-scroll", this); m_autoScroll->setChecked(true);
    auto* clearBtn = new QPushButton("Clear", this);
    toolbar->addWidget(m_showDebug);
    toolbar->addWidget(m_showInfo);
    toolbar->addWidget(m_showWarn);
    toolbar->addWidget(m_showError);
    toolbar->addWidget(m_autoScroll);
    toolbar->addWidget(clearBtn);
    toolbar->addStretch();
    layout->addLayout(toolbar);

    m_textEdit = new QTextEdit(this);
    m_textEdit->setReadOnly(true);
    m_textEdit->setAcceptRichText(true);
    QFont mono("Courier New", font().pointSize());
    m_textEdit->setFont(mono);
    layout->addWidget(m_textEdit);

    connect(clearBtn, &QPushButton::clicked, this, [this]() {
        m_state.logs.clear();
        m_textEdit->clear();
        m_lastSize = 0;
    });

    auto refilter = [this]() {
        m_textEdit->clear();
        m_lastSize = 0;
        refresh();
    };
    connect(m_showDebug, &QCheckBox::toggled, this, refilter);
    connect(m_showInfo,  &QCheckBox::toggled, this, refilter);
    connect(m_showWarn,  &QCheckBox::toggled, this, refilter);
    connect(m_showError, &QCheckBox::toggled, this, refilter);
}

void LogsPanel::refresh() {
    auto entries = m_state.logs.snapshot();
    size_t newSize = entries.size();

    if (newSize == m_lastSize) return;

    bool debug = m_showDebug->isChecked();
    bool info  = m_showInfo->isChecked();
    bool warn  = m_showWarn->isChecked();
    bool error = m_showError->isChecked();

    if (newSize < m_lastSize) {
        m_textEdit->clear();
        m_lastSize = 0;
    }

    for (size_t i = m_lastSize; i < newSize; ++i) {
        const auto& e = entries[i];
        if (!shouldShow(e.level, debug, info, warn, error)) continue;
        QString color = levelColor(e.level);
        QString msg   = QString::fromStdString(e.message).toHtmlEscaped();
        m_textEdit->append(
            QString("<span style=\"color:%1\">%2</span>").arg(color, msg));
    }
    m_lastSize = newSize;

    if (m_autoScroll->isChecked()) {
        auto* sb = m_textEdit->verticalScrollBar();
        sb->setValue(sb->maximum());
    }
}
