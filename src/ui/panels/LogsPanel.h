#pragma once
#include "ui/IPanel.h"
#include <cstddef>
#include "core/AppState.h"

class QCheckBox;
class QTextEdit;

class LogsPanel : public IPanel {
public:
    explicit LogsPanel(AppState& state, QWidget* parent = nullptr);
    void refresh() override;

private:
    AppState& m_state;

    QCheckBox* m_showDebug  = nullptr;
    QCheckBox* m_showInfo   = nullptr;
    QCheckBox* m_showWarn   = nullptr;
    QCheckBox* m_showError  = nullptr;
    QCheckBox* m_autoScroll = nullptr;
    QTextEdit* m_textEdit   = nullptr;
    size_t     m_lastSize   = 0;
};
