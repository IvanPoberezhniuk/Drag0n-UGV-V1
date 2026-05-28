#pragma once
#include <QWidget>

class CompassBar : public QWidget {
public:
    explicit CompassBar(QWidget* parent = nullptr);
    void setHeading(float degrees, bool valid);

protected:
    void paintEvent(QPaintEvent*) override;

private:
    float m_heading = 0.f;
    bool  m_valid   = false;
};
