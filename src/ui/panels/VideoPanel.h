#pragma once
#include <QWidget>

class VideoPanel : public QWidget {
    Q_OBJECT
public:
    explicit VideoPanel(QWidget* parent = nullptr);

protected:
    void paintEvent(QPaintEvent*) override;
};
