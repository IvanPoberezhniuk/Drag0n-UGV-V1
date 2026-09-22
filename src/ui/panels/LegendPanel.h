#pragma once
#include "ui/IPanel.h"
#include <QColor>
#include <functional>
#include <vector>
#include "core/AppState.h"

class LegendPanel : public IPanel {
public:
    explicit LegendPanel(AppState& state, QWidget* parent = nullptr);
    void refresh() override;
    QSize sizeHint() const override;

protected:
    void paintEvent(QPaintEvent*) override;

private:
    // Each entry draws itself at a given top-left (x, y) and reports how
    // tall it is, so paintEvent can flow entries top-to-bottom within a
    // column and wrap to the next column once the panel's height runs out --
    // the panel is now much wider than tall, so a single vertical list
    // would leave most of its width empty.
    struct Entry {
        int height;
        std::function<void(QPainter&, int, int)> draw;
    };

    std::vector<Entry> buildKeyboardEntries(int K, int G, int AX) const;
    std::vector<Entry> buildControllerEntries(int K, int G, int AX) const;

    void drawKey(QPainter& p, QRectF r, const QString& label) const;
    void drawCircularKey(QPainter& p, int x, int y, int size,
                                const QString& label, QColor fill) const;
    void drawSectionTitle(QPainter& p, int x, int y, const QString& text) const;

    AppState&  m_state;
    InputType  m_lastInput = InputType::Keyboard;
};
