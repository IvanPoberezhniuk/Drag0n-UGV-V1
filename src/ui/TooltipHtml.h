#pragma once
#include <QString>
#include <QList>
#include <QPair>

// Builds a small two-column tooltip: a bold title naming the hovered item,
// then a name/value row per detail (name left, value right-aligned). Shared
// by DashboardBar and HudCrosshair so every hover tooltip in the HUD looks
// the same, and styled via QToolTip's palette (see Theme::darkPalette) to
// match the rest of the UI instead of the OS default tooltip look.
inline QString tooltipHtml(const QString& title, const QList<QPair<QString, QString>>& rows = {}) {
    QString html = QString("<b>%1</b>").arg(title.toHtmlEscaped());
    if (!rows.isEmpty()) {
        html += "<table cellspacing='0' cellpadding='0' width='100%'>";
        for (const auto& row : rows) {
            html += QString("<tr><td>%1</td><td align='right' style='padding-left:6px;'>%2</td></tr>")
                .arg(row.first.toHtmlEscaped(), row.second.toHtmlEscaped());
        }
        html += "</table>";
    }
    return html;
}
