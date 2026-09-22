#pragma once
#include "ui/widgets/HoverHitRegions.h"
#include <QWidget>
#include <QColor>
#include <QPixmap>
#include <QHash>
#include <QString>
#include <QTimer>
#include <QElapsedTimer>
#include <QRect>
#include <QPair>

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

    // Module status (left group).
    //
    // GPS/velocity-sensor/speaker have no backend data source at all yet (no
    // such hardware wired into telemetry). "Enabled" records only the
    // operator's preference: off is gray+crossed, while enabled but
    // unavailable is plain gray and cannot be clicked. A disabled item stays
    // clickable so the operator can enable it again. Once an availability
    // source is integrated, enabled+available becomes amber and clickable.
    void setGpsWatchEnabled(bool enabled);
    void setVelocityWatchEnabled(bool enabled);
    void setSpeakerWatchEnabled(bool enabled);

    // Camera is the one module-status icon with a real connected/not signal
    // (VideoWorker/CameraState). setCameraEnabled reflects the user's
    // on/off toggle (gray+crossed when off); setCameraStreaming reflects
    // actual stream health while enabled (amber blink = streaming, plain
    // gray = no signal).
    void setCameraEnabled(bool enabled);
    void setCameraStreaming(bool streaming); // true = actively sharing an image

    // ESP/STM32 nodes are not part of the click-to-toggle feature -- amber
    // when online, red when offline, never gray, not clickable.
    void setEspOk(bool ok);
    void setStmLeftOk(bool ok);
    void setStmRightOk(bool ok);

    // Extra name/value rows appended to the ESP/STM tooltips below the
    // online/offline status (e.g. RSSI, link state, fault mask) -- callers
    // pass an empty list when there's nothing more specific to say.
    using DetailRows = QList<QPair<QString, QString>>;
    void setCameraDetail(const DetailRows& rows);
    void setEspDetail(const DetailRows& rows);
    void setStmLeftDetail(const DetailRows& rows);
    void setStmRightDetail(const DetailRows& rows);

signals:
    // Emitted on release of a completed click (press and release both
    // landing on the same icon) for each of the four toggleable
    // module-status icons. Argument is the new (post-toggle) state.
    void cameraToggled(bool enabled);
    void gpsWatchToggled(bool enabled);
    void velocityWatchToggled(bool enabled);
    void speakerWatchToggled(bool enabled);

protected:
    void paintEvent(QPaintEvent*) override;
    void mouseMoveEvent(QMouseEvent* e) override;
    void leaveEvent(QEvent* e) override;
    void mousePressEvent(QMouseEvent* e) override;
    void mouseReleaseEvent(QMouseEvent* e) override;

private:
    bool  m_cruiseEnabled = false;
    float m_cruiseSpeed   = 0.0f;
    bool  m_estopActive   = false;
    bool  m_lightsOn      = false;

    bool m_gpsWatchEnabled      = false;
    bool m_velocityWatchEnabled = false;
    bool m_speakerWatchEnabled  = false;
    bool m_cameraEnabled        = true;
    bool m_cameraStreaming      = false;
    bool m_espOk                = false;
    bool m_stmLeftOk            = false;
    bool m_stmRightOk           = false;

    // Click hit-rects for the first four (toggleable) status icons, in the
    // same gps/velocity/speaker/camera order as statusIcons[] in
    // paintEvent -- rebuilt every paintEvent alongside the hover rects.
    QRect m_toggleRects[4];
    int   m_pressedToggleIndex = -1;

    DetailRows m_espDetail;
    DetailRows m_stmLeftDetail;
    DetailRows m_stmRightDetail;
    DetailRows m_cameraDetail;

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
    // tooltip off of). See HoverHitRegions for why re-showing is guarded.
    mutable HoverHitRegions m_hover;

    static QByteArray loadSvgTemplate(const QString& resourcePath);
    QPixmap coloredIcon(const QByteArray& svgTemplate, const QString& cacheKeyPrefix,
                       const QColor& color, int sizePx) const;
    QColor cameraBlinkColor() const;
};
