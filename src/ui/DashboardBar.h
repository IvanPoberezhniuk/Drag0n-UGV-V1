#pragma once
#include <QWidget>
#include <QColor>
#include <QPixmap>
#include <QHash>
#include <QString>
#include <QTimer>
#include <QElapsedTimer>
#include <QRect>
#include <vector>

// Bottom overlay strip on the video feed. Two groups:
//  - left: module status icons (GPS, velocity sensor, speaker, camera, ESP,
//    2x STM32) -- health-only, icon color conveys ok/down.
//  - center: car-dashboard-style control icons (cruise, E-Stop, lights).
// Visual style mirrors CompassBar (translucent dark strip). Icons are Tabler
// Icons SVGs (resources/icons/, MIT license) recolored per state at render
// time. Every icon shows a hover tooltip naming the device/control it
// represents (see m_hitRects, populated each paintEvent).
class DashboardBar : public QWidget {
    Q_OBJECT
public:
    explicit DashboardBar(QWidget* parent = nullptr);

    void setCruise(bool enabled, float speedFraction);
    void setEstop(bool active);
    void setLights(bool on);

    // Module status (left group). GPS/velocity-sensor/speaker/camera have no
    // backend data source yet (no such hardware wired into telemetry) --
    // callers pass false until that lands; the UI is complete and ready.
    void setGpsOk(bool ok);
    void setVelocitySensorOk(bool ok);
    void setSpeakerOk(bool ok);
    void setCameraStreaming(bool streaming); // true = actively sharing an image
    void setEspOk(bool ok);
    void setStmLeftOk(bool ok);
    void setStmRightOk(bool ok);

protected:
    void paintEvent(QPaintEvent*) override;
    bool event(QEvent* e) override;

private:
    bool  m_cruiseEnabled = false;
    float m_cruiseSpeed   = 0.0f;
    bool  m_estopActive   = false;
    bool  m_lightsOn      = false;

    bool m_gpsOk            = false;
    bool m_velocitySensorOk = false;
    bool m_speakerOk        = false;
    bool m_cameraStreaming  = false;
    bool m_espOk            = false;
    bool m_stmLeftOk        = false;
    bool m_stmRightOk       = false;

    QByteArray m_cruiseSvg;
    QByteArray m_estopSvg;
    QByteArray m_lightsSvg;
    QByteArray m_lightsOffSvg;
    QByteArray m_gpsSvg;
    QByteArray m_velocitySvg;
    QByteArray m_speakerSvg;
    QByteArray m_cameraSvg;
    QByteArray m_espSvg;
    QByteArray m_stmSvg;

    mutable QHash<QString, QPixmap> m_pixmapCache;

    QTimer        m_blinkTimer;   // only runs while m_cameraStreaming
    QElapsedTimer m_blinkClock;

    // Icon hover tooltips: hit-test rects rebuilt on every paintEvent, since
    // icon layout is computed there (no child widgets to hang a native
    // tooltip off of).
    struct IconHitRect { QRect rect; QString tooltip; };
    mutable std::vector<IconHitRect> m_hitRects;

    static QByteArray loadSvgTemplate(const QString& resourcePath);
    QPixmap coloredIcon(const QByteArray& svgTemplate, const QString& cacheKeyPrefix,
                       const QColor& color, int sizePx) const;
    QColor cameraBlinkColor() const;
};
