#pragma once
#include "ui/IPanel.h"
#include <cstddef>
#include "core/AppState.h"

class QCheckBox;
class QTextEdit;
class CheckableComboBox;

class LogsPanel : public IPanel {
public:
    explicit LogsPanel(AppState& state, QWidget* parent = nullptr);
    void refresh() override;

private:
    AppState& m_state;

    // Level filter: DEBUG/INFO/WARN/ERROR checkboxes collapsed under a
    // single checkable dropdown instead of 4 separate widgets.
    CheckableComboBox* m_levelFilter = nullptr;
    QCheckBox* m_autoScroll = nullptr;
    QTextEdit* m_textEdit   = nullptr;
    size_t     m_lastSize   = 0;
};
