#pragma once
#include "ui/widgets/HudTooltip.h"
#include <QMouseEvent>
#include <QRect>
#include <QString>
#include <vector>

// Hit-tests a set of rects against the mouse position and shows/hides a
// shared HudTooltip for whichever one (if any) is hovered. Shared by
// custom-painted widgets that rebuild their hit-rects every paintEvent
// (DashboardBar, HudCrosshair), since they have no child widgets to hang a
// native tooltip off of.
class HoverHitRegions {
public:
    void clear() { m_regions.clear(); }
    void add(const QRect& rect, const QString& tooltip) { m_regions.push_back({rect, tooltip}); }

    void handleMouseMove(const QMouseEvent& e) {
        for (const auto& r : m_regions) {
            if (r.rect.contains(e.pos())) {
                if (r.rect != m_hovered) {
                    m_hovered = r.rect;
                    HudTooltip::showText(e.globalPosition().toPoint(), r.tooltip);
                }
                return;
            }
        }
        handleLeave();
    }

    void handleLeave() {
        m_hovered = QRect();
        HudTooltip::hideText();
    }

private:
    struct Region { QRect rect; QString tooltip; };
    std::vector<Region> m_regions;
    QRect m_hovered;
};
