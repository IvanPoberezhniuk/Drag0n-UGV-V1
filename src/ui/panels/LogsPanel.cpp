#include "ui/panels/LogsPanel.h"
#include "core/LogBuffer.h"
#include "ui/Theme.h"
#include "ui/widgets/CheckableComboBox.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QCheckBox>
#include <QTextEdit>
#include <QPushButton>
#include <QIcon>
#include <QFont>
#include <QScrollBar>
#include <spdlog/spdlog.h>

static QString levelColor(spdlog::level::level_enum lvl) {
    switch (lvl) {
        case spdlog::level::debug:    return Theme::textDim.name();
        case spdlog::level::info:     return Theme::textPrimary.name();
        case spdlog::level::warn:     return Theme::warningYellow.name();
        case spdlog::level::err:
        case spdlog::level::critical: return Theme::errorRed.name();
        default:                      return Theme::textPrimary.name();
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
    : IPanel(parent), m_state(state)
{
    auto* layout = new QVBoxLayout(this);

    auto* toolbar = new QHBoxLayout;
    m_levelFilter = new CheckableComboBox(this);
    m_levelFilter->setTitleText("Levels");
    m_levelFilter->addCheckableItem("DEBUG");
    m_levelFilter->addCheckableItem("INFO");
    m_levelFilter->addCheckableItem("WARN");
    m_levelFilter->addCheckableItem("ERROR");
    m_levelFilter->setMinimumWidth(90);
    m_autoScroll = new QCheckBox("Auto-scroll", this); m_autoScroll->setChecked(true);
    auto* clearBtn = new QPushButton(this);
    clearBtn->setIcon(QIcon(":/icons/trash.svg"));
    clearBtn->setToolTip("Clear logs");
    clearBtn->setFixedSize(30, 26);
    toolbar->addWidget(m_levelFilter);
    toolbar->addWidget(m_autoScroll);
    toolbar->addStretch();
    toolbar->addWidget(clearBtn);
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
    connect(m_levelFilter, &CheckableComboBox::checkedChanged, this, refilter);
}

void LogsPanel::refresh() {
    auto entries = m_state.logs.snapshot();
    size_t newSize = entries.size();

    if (newSize == m_lastSize) return;

    bool debug = m_levelFilter->isChecked(0);
    bool info  = m_levelFilter->isChecked(1);
    bool warn  = m_levelFilter->isChecked(2);
    bool error = m_levelFilter->isChecked(3);

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
