#pragma once
#include "ui/IPanel.h"
#include <QColor>
#include "core/AppState.h"

class LegendPanel : public IPanel {
public:
    explicit LegendPanel(AppState& state, QWidget* parent = nullptr);
    void refresh() override;
    QSize sizeHint() const override;

protected:
    void paintEvent(QPaintEvent*) override;

private:
    void drawKeyboard(QPainter& p, int x, int& y);
    void drawController(QPainter& p, int x, int& y);

    void drawKey(QPainter& p, QRectF r, const QString& label);
    void drawCircularKey(QPainter& p, int x, int y, int size,
                                const QString& label, QColor fill);
    void drawSectionTitle(QPainter& p, int x, int y, const QString& text);

    AppState&  m_state;
    InputType  m_lastInput = InputType::Keyboard;
};
