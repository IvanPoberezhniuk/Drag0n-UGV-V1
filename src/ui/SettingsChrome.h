#pragma once
#include "ui/Theme.h"
#include <QColor>
#include <QString>

// Look shared by the settings-style dialogs (SettingsDialog, RadioSettingsDialog):
// a dark sidebar list with an accent bar on the selected row.
namespace SettingsChrome {

static const QColor sidebarBg        {  42,  42,  42 };
static const QColor sidebarBorder    {  68,  68,  68 };
static const QColor sidebarItemText  { 204, 204, 204 };
static const QColor sidebarSelectedBg{  58,  58,  58 };

inline QString sidebarStyleSheet() {
    return QString(
        "QListWidget { background: %1; border-right: 1px solid %2; }"
        "QListWidget::item { padding: 10px 12px; color: %3; }"
        "QListWidget::item:selected { background: %4; color: white; "
        "  border-left: 3px solid %5; }")
        .arg(sidebarBg.name(), sidebarBorder.name(), sidebarItemText.name(),
             sidebarSelectedBg.name(), Theme::accent.name());
}

} // namespace SettingsChrome
