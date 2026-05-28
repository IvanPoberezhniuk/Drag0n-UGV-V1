#pragma once
#include "core/AppState.h"
#include <QWidget>
#include <QImage>
#include <random>

class WheelPanel;
class CompassBar;

class VideoPanel : public QWidget {
    Q_OBJECT
public:
    explicit VideoPanel(AppState& state, QWidget* parent = nullptr);
    void refresh();

protected:
    void paintEvent(QPaintEvent*) override;
    void resizeEvent(QResizeEvent*) override;

private:
    AppState&    m_state;
    WheelPanel*  m_wheels  = nullptr;
    CompassBar*  m_compass = nullptr;
    QImage       m_noise;
    std::mt19937 m_rng{ std::random_device{}() };

    void repositionWheels();
    void repositionCompass();
    void generateNoise();
};
