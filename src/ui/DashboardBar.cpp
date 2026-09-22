#include "ui/DashboardBar.h"
#include "ui/Theme.h"
#include "ui/TooltipHtml.h"
#include <QPainter>
#include <QFile>
#include <QSvgRenderer>
#include <QMouseEvent>
#include <algorithm>

static constexpr int kIconSize    = 26;
static constexpr int kItemGap     = 11; // between items
static constexpr int kHoverPad    = 5;  // hover hit-area grows this far past the icon's own pixels
static constexpr int kGroupMargin = 8;  // left group's distance from the bar's left edge
static constexpr int kBlinkPeriodMs = 1600; // full ease-in-out cycle while camera streams

DashboardBar::DashboardBar(QWidget* parent)
    : QWidget(parent)
{
    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_NoSystemBackground);
    // Needed for QEvent::ToolTip to fire reliably on a plain custom-painted
    // QWidget -- without it Qt only forwards mouse-move while a button is
    // held, so the tooltip timer never restarts as the pointer drifts
    // between icon hit-rects.
    setMouseTracking(true);
    // Height is set directly by VideoPanel::repositionDashboard() (which
    // intentionally overshoots the bottom edge) rather than fixed here.

    m_cruiseSvg    = loadSvgTemplate(":/icons/cruise.svg");
    m_estopSvg     = loadSvgTemplate(":/icons/estop.svg");
    m_lightsSvg    = loadSvgTemplate(":/icons/lights.svg");
    m_lightsOffSvg = loadSvgTemplate(":/icons/lights-off.svg");
    m_gpsSvg       = loadSvgTemplate(":/icons/gps.svg");
    m_velocitySvg  = loadSvgTemplate(":/icons/velocity.svg");
    m_speakerSvg   = loadSvgTemplate(":/icons/speaker.svg");
    m_cameraSvg    = loadSvgTemplate(":/icons/camera.svg");
    m_espSvg       = loadSvgTemplate(":/icons/esp-status.svg");
    m_stmSvg       = loadSvgTemplate(":/icons/stm-status.svg");

    m_blinkTimer.setInterval(Theme::kUiRefreshMs); // only runs while streaming
    connect(&m_blinkTimer, &QTimer::timeout, this, [this]() { update(); });
}

QByteArray DashboardBar::loadSvgTemplate(const QString& resourcePath) {
    QFile f(resourcePath);
    if (!f.open(QIODevice::ReadOnly)) {
        return QByteArray();
    }
    return f.readAll();
}

QPixmap DashboardBar::coloredIcon(const QByteArray& svgTemplate, const QString& cacheKeyPrefix,
                                  const QColor& color, int sizePx) const {
    const qreal dpr = devicePixelRatioF();
    const int pixelSize = qRound(sizePx * dpr);
    const QString key = cacheKeyPrefix + color.name(QColor::HexArgb) + QString::number(pixelSize);

    auto it = m_pixmapCache.find(key);
    if (it != m_pixmapCache.end()) {
        return it.value();
    }

    QByteArray svg = svgTemplate;
    // The stroke is baked in as opaque RGB (HexRgb drops alpha) -- the
    // color's alpha channel is applied afterwards via painter opacity below,
    // since an alpha value embedded in the SVG source itself would be
    // ignored by "currentColor" substitution here anyway.
    svg.replace("currentColor", color.name(QColor::HexRgb).toUtf8());

    QPixmap pm(pixelSize, pixelSize);
    pm.fill(Qt::transparent);
    QSvgRenderer renderer(svg);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing);
    p.setOpacity(color.alphaF());
    renderer.render(&p, QRectF(0, 0, pixelSize, pixelSize));
    p.end();
    pm.setDevicePixelRatio(dpr);

    m_pixmapCache.insert(key, pm);
    return pm;
}

QColor DashboardBar::cameraBlinkColor() const {
    const qint64 elapsed = m_blinkClock.isValid() ? m_blinkClock.elapsed() : 0;
    const double ease = Theme::pulsePhase(elapsed, kBlinkPeriodMs);
    const QColor dim(90, 60, 0);
    const QColor bright = Theme::accent;
    const auto lerp = [ease](int a, int b) { return a + static_cast<int>((b - a) * ease); };
    return QColor(lerp(dim.red(), bright.red()),
                  lerp(dim.green(), bright.green()),
                  lerp(dim.blue(), bright.blue()));
}

void DashboardBar::setCruise(bool enabled, float speedFraction) {
    m_cruiseEnabled = enabled;
    m_cruiseSpeed   = speedFraction;
    update();
}

void DashboardBar::setEstop(bool active) {
    m_estopActive = active;
    update();
}

void DashboardBar::setLights(bool on) {
    m_lightsOn = on;
    update();
}

void DashboardBar::setGpsWatchEnabled(bool enabled) {
    m_gpsWatchEnabled = enabled;
    update();
}

void DashboardBar::setVelocityWatchEnabled(bool enabled) {
    m_velocityWatchEnabled = enabled;
    update();
}

void DashboardBar::setSpeakerWatchEnabled(bool enabled) {
    m_speakerWatchEnabled = enabled;
    update();
}

void DashboardBar::setCameraEnabled(bool enabled) {
    m_cameraEnabled = enabled;
    update();
}

void DashboardBar::setCameraStreaming(bool streaming) {
    if (streaming == m_cameraStreaming) return;
    m_cameraStreaming = streaming;
    if (streaming) {
        m_blinkClock.start();
        m_blinkTimer.start();
    } else {
        m_blinkTimer.stop();
    }
    update();
}

void DashboardBar::setEspOk(bool ok) {
    m_espOk = ok;
    update();
}

void DashboardBar::setStmLeftOk(bool ok) {
    m_stmLeftOk = ok;
    update();
}

void DashboardBar::setStmRightOk(bool ok) {
    m_stmRightOk = ok;
    update();
}

void DashboardBar::setEspDetail(const DetailRows& rows) {
    m_espDetail = rows;
    update();
}

void DashboardBar::setStmLeftDetail(const DetailRows& rows) {
    m_stmLeftDetail = rows;
    update();
}

void DashboardBar::setStmRightDetail(const DetailRows& rows) {
    m_stmRightDetail = rows;
    update();
}

void DashboardBar::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    m_hover.clear();

    const int W = width();
    const int H = height();

    // Semi-transparent background strip, same tint as CompassBar
    p.setPen(Qt::NoPen);
    p.setBrush(Theme::hudStripBackground);
    p.drawRect(0, 0, W, H);

    // --- Left group: module status icons, health-only (no text). ---
    // Color scheme: amber = connected/on, red = genuine error, gray = "not
    // important"/disabled. GPS/velocity/speaker have no backend data source
    // at all (no such hardware wired into telemetry) -- they're honest
    // UI-preference toggles with no fail state: gray+crossed when off
    // (their permanent default), plain amber when on. Camera is the one
    // module icon among the four with a real connected/not-connected
    // signal: gray+crossed when the user disabled it, red when enabled but
    // not streaming, amber blink when actually streaming. ESP/STM32 are not
    // toggleable: amber online, red offline, never gray.
    struct StatusIcon {
        const char* cacheKey;
        const QByteArray* svg;
        QColor color;
        QString tooltip;
        bool crossed = false;
    };
    // A touch of transparency on every icon color keeps the strip from
    // reading as harshly saturated against the video feed.
    auto soften = [](QColor c) { c.setAlpha(215); return c; };
    const QColor onColor   = soften(Theme::accent);
    const QColor critColor = soften(Theme::errorRed);
    // "Not important"/disabled gray, consistent with the dim gray already
    // used below for inactive cruise/estop/lights.
    const QColor offColor(120, 120, 120, 170);

    const QString gpsStatus = m_gpsWatchEnabled ? "On" : "Off (click to enable)";
    const QString velStatus = m_velocityWatchEnabled ? "On" : "Off (click to enable)";
    const QString spkStatus = m_speakerWatchEnabled ? "On" : "Off (click to enable)";
    const QString camStatus = !m_cameraEnabled ? "Off (click to enable)"
                             : m_cameraStreaming ? "Streaming (click to disable)"
                                                  : "No signal (click to disable)";

    StatusIcon statusIcons[7] = {
        { "gps", &m_gpsSvg,      m_gpsWatchEnabled      ? onColor : offColor,
          tooltipHtml("GPS Module", {{"Status", gpsStatus}}), !m_gpsWatchEnabled },
        { "vel", &m_velocitySvg, m_velocityWatchEnabled ? onColor : offColor,
          tooltipHtml("Velocity Sensor", {{"Status", velStatus}}), !m_velocityWatchEnabled },
        { "spk", &m_speakerSvg,  m_speakerWatchEnabled  ? onColor : offColor,
          tooltipHtml("Speaker", {{"Status", spkStatus}}), !m_speakerWatchEnabled },
        { "cam", &m_cameraSvg,
          !m_cameraEnabled ? offColor : (m_cameraStreaming ? soften(cameraBlinkColor()) : critColor),
          tooltipHtml("Camera", {{"Status", camStatus}}), !m_cameraEnabled },
        { "esp", &m_espSvg,      m_espOk            ? onColor : critColor,
          tooltipHtml("ESP32 Controller",
              DetailRows{{"Status", m_espOk ? "Online" : "Offline"}} + m_espDetail) },
        { "stmL", &m_stmSvg,     m_stmLeftOk        ? onColor : critColor,
          tooltipHtml("STM32 Left Node",
              DetailRows{{"Status", m_stmLeftOk ? "Online" : "Offline"}} + m_stmLeftDetail) },
        { "stmR", &m_stmSvg,     m_stmRightOk       ? onColor : critColor,
          tooltipHtml("STM32 Right Node",
              DetailRows{{"Status", m_stmRightOk ? "Online" : "Offline"}} + m_stmRightDetail) },
    };
    {
        int sx = kGroupMargin;
        for (int i = 0; i < 7; ++i) {
            const auto& icon = statusIcons[i];
            QRect box(sx, qRound((H - kIconSize) / 2.0), kIconSize, kIconSize);
            p.drawPixmap(box, coloredIcon(*icon.svg, icon.cacheKey, icon.color, kIconSize));
            if (icon.crossed) {
                QPen pen(offColor.lighter(140));
                pen.setWidth(2);
                p.setPen(pen);
                p.drawLine(box.topLeft(), box.bottomRight());
            }
            const QRect hitBox = box.adjusted(-kHoverPad, -kHoverPad, kHoverPad, kHoverPad);
            m_hover.add(hitBox, icon.tooltip);
            if (i < 4) m_toggleRects[i] = hitBox; // gps, velocity, speaker, camera
            sx += kIconSize + kItemGap;
        }
    }

    // --- Center group: cruise / E-Stop / lights, icon-only. ---
    const QColor dim(150, 150, 150, 170);

    struct Item {
        const QByteArray* svg;
        QColor  color;
        QString tooltip;
    };

    Item items[3] = {
        { &m_cruiseSvg, m_cruiseEnabled ? Theme::warningYellow : dim,
          tooltipHtml("Cruise Control", {{"Status", m_cruiseEnabled
              ? QString("ON (%1%)").arg(qRound(m_cruiseSpeed * 100)) : "OFF"}}) },
        { &m_estopSvg, m_estopActive ? Theme::errorRed : dim,
          tooltipHtml("Emergency Stop", {{"Status", m_estopActive ? "ACTIVE" : "Clear"}}) },
        { m_lightsOn ? &m_lightsSvg : &m_lightsOffSvg,
          m_lightsOn ? Theme::accent : dim,
          tooltipHtml("Lights", {{"Status", m_lightsOn ? "ON" : "OFF"}}) },
    };

    constexpr int kItemCount = 3;
    const int totalW = kItemCount * kIconSize + (kItemCount - 1) * kItemGap;

    int x = (W - totalW) / 2;
    for (int i = 0; i < kItemCount; ++i) {
        QRect iconBox(x, qRound((H - kIconSize) / 2.0), kIconSize, kIconSize);
        const QPixmap icon = coloredIcon(*items[i].svg, QString::number(i), items[i].color, kIconSize);
        p.drawPixmap(iconBox, icon);
        m_hover.add(iconBox.adjusted(-kHoverPad, -kHoverPad, kHoverPad, kHoverPad), items[i].tooltip);

        x += kIconSize + kItemGap;
    }
}

void DashboardBar::mouseMoveEvent(QMouseEvent* e) {
    m_hover.handleMouseMove(*e);
}

void DashboardBar::leaveEvent(QEvent*) {
    m_hover.handleLeave();
}

void DashboardBar::mousePressEvent(QMouseEvent* e) {
    m_pressedToggleIndex = -1;
    if (e->button() == Qt::LeftButton) {
        for (int i = 0; i < 4; ++i) {
            if (m_toggleRects[i].contains(e->pos())) {
                m_pressedToggleIndex = i;
                break;
            }
        }
    }
    QWidget::mousePressEvent(e);
}

void DashboardBar::mouseReleaseEvent(QMouseEvent* e) {
    const int pressed = m_pressedToggleIndex;
    m_pressedToggleIndex = -1;
    if (e->button() == Qt::LeftButton && pressed >= 0 &&
        m_toggleRects[pressed].contains(e->pos())) {
        switch (pressed) {
            case 0: emit gpsWatchToggled(!m_gpsWatchEnabled); break;
            case 1: emit velocityWatchToggled(!m_velocityWatchEnabled); break;
            case 2: emit speakerWatchToggled(!m_speakerWatchEnabled); break;
            case 3: emit cameraToggled(!m_cameraEnabled); break;
            default: break;
        }
    }
    QWidget::mouseReleaseEvent(e);
}
