#pragma once
#include <QWidget>
#include <QColor>

class LegendPanel : public QWidget {
public:
    explicit LegendPanel(QWidget* parent = nullptr);
    QSize sizeHint() const override;

protected:
    void paintEvent(QPaintEvent*) override;

private:
    void drawKeyboard(QPainter& p, int x, int& y);
    void drawController(QPainter& p, int x, int& y);

    static void drawKey(QPainter& p, QRectF r, const QString& label);
    static void drawCircularKey(QPainter& p, int x, int y, int size,
                                const QString& label, QColor fill);
    static void drawSectionTitle(QPainter& p, int x, int y, const QString& text);
};
