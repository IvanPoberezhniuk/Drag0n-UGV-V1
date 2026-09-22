#pragma once
#include <QWidget>
#include <QByteArray>
#include <QPixmap>

class CompassBar : public QWidget {
public:
    explicit CompassBar(QWidget* parent = nullptr);
    void setHeading(float degrees, bool valid);

protected:
    void paintEvent(QPaintEvent*) override;

private:
    float m_heading = 0.f;
    bool  m_valid   = false;

    // Shown centered in place of the tape when heading is unavailable (no
    // GPS fix / compass can't resolve azimuth) -- Tabler "navigation-off",
    // same recoloring approach as DashboardBar::coloredIcon().
    QByteArray    m_navOffSvg;
    mutable QPixmap m_navOffPixmap;
};
