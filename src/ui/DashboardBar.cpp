#include "ui/DashboardBar.h"
#include "ui/Theme.h"
#include <QPainter>
#include <QFile>
#include <QSvgRenderer>
#include <QHelpEvent>
#include <QToolTip>
#include <algorithm>
#include <cmath>

static constexpr int kIconSize    = 26;
static constexpr int kItemGap     = 11; // between items
static constexpr int kGroupMargin = 8;  // left group's distance from the bar's left edge
static constexpr double kPi       = 3.14159265358979323846;
static constexpr int kBlinkPeriodMs = 1600; // full ease-in-out cycle while camera streams

DashboardBar::DashboardBar(QWidget* parent)
    : QWidget(parent)
{
    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_NoSystemBackground);
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

    m_blinkTimer.setInterval(33); // ~30fps, only runs while streaming
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
    svg.replace("currentColor", color.name(QColor::HexRgb).toUtf8());

    QPixmap pm(pixelSize, pixelSize);
    pm.fill(Qt::transparent);
    QSvgRenderer renderer(svg);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing);
    renderer.render(&p, QRectF(0, 0, pixelSize, pixelSize));
    p.end();
    pm.setDevicePixelRatio(dpr);

    m_pixmapCache.insert(key, pm);
    return pm;
}

QColor DashboardBar::cameraBlinkColor() const {
    const qint64 elapsed = m_blinkClock.isValid() ? m_blinkClock.elapsed() : 0;
    const double phase = std::fmod(static_cast<double>(elapsed), kBlinkPeriodMs) / kBlinkPeriodMs;
    const double ease = 0.5 - 0.5 * std::cos(2.0 * kPi * phase); // 0..1 ease-in-out
    const QColor dim(30, 90, 180);
    const QColor bright = Theme::infoBlue;
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

void DashboardBar::setGpsOk(bool ok) {
    m_gpsOk = ok;
    update();
}

void DashboardBar::setVelocitySensorOk(bool ok) {
    m_velocitySensorOk = ok;
    update();
}

void DashboardBar::setSpeakerOk(bool ok) {
    m_speakerOk = ok;
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

void DashboardBar::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    m_hitRects.clear();

    const int W = width();
    const int H = height();

    // Semi-transparent background strip, same tint as CompassBar
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(0, 0, 0, 130));
    p.drawRect(0, 0, W, H);

    // --- Left group: module status icons, health-only (no text). ---
    // GPS/velocity-sensor/speaker/camera have no backend data source yet (no
    // such hardware wired into telemetry) -- callers pass false/off until
    // that lands. Non-critical modules show caution orange when down;
    // critical ones (ESP/STM32 nodes) show red.
    struct StatusIcon {
        const char* cacheKey;
        const QByteArray* svg;
        QColor color;
        QString tooltip;
    };
    const QColor okColor = Theme::successGreen;
    StatusIcon statusIcons[7] = {
        { "gps", &m_gpsSvg,      m_gpsOk            ? okColor : Theme::cautionOrange, "GPS module" },
        { "vel", &m_velocitySvg, m_velocitySensorOk ? okColor : Theme::cautionOrange, "Velocity sensor" },
        { "spk", &m_speakerSvg,  m_speakerOk        ? okColor : Theme::cautionOrange, "Speaker" },
        { "cam", &m_cameraSvg,   m_cameraStreaming  ? cameraBlinkColor() : Theme::cautionOrange, "Camera stream" },
        { "esp", &m_espSvg,      m_espOk            ? okColor : Theme::errorRed, "ESP32 controller" },
        { "stmL", &m_stmSvg,     m_stmLeftOk        ? okColor : Theme::errorRed, "STM32 left node" },
        { "stmR", &m_stmSvg,     m_stmRightOk       ? okColor : Theme::errorRed, "STM32 right node" },
    };
    {
        int sx = kGroupMargin;
        for (const auto& icon : statusIcons) {
            QRect box(sx, qRound((H - kIconSize) / 2.0), kIconSize, kIconSize);
            p.drawPixmap(box, coloredIcon(*icon.svg, icon.cacheKey, icon.color, kIconSize));
            m_hitRects.push_back({box, icon.tooltip});
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
        { &m_cruiseSvg, m_cruiseEnabled ? Theme::warningYellow : dim, "Cruise control" },
        { &m_estopSvg, m_estopActive ? Theme::errorRed : dim, "Emergency stop" },
        { m_lightsOn ? &m_lightsSvg : &m_lightsOffSvg,
          m_lightsOn ? Theme::infoBlue : dim, "Lights" },
    };

    constexpr int kItemCount = 3;
    const int totalW = kItemCount * kIconSize + (kItemCount - 1) * kItemGap;

    int x = (W - totalW) / 2;
    for (int i = 0; i < kItemCount; ++i) {
        QRect iconBox(x, qRound((H - kIconSize) / 2.0), kIconSize, kIconSize);
        const QPixmap icon = coloredIcon(*items[i].svg, QString::number(i), items[i].color, kIconSize);
        p.drawPixmap(iconBox, icon);
        m_hitRects.push_back({iconBox, items[i].tooltip});

        x += kIconSize + kItemGap;
    }
}

bool DashboardBar::event(QEvent* e) {
    if (e->type() == QEvent::ToolTip) {
        auto* he = static_cast<QHelpEvent*>(e);
        for (const auto& hit : m_hitRects) {
            if (hit.rect.contains(he->pos())) {
                QToolTip::showText(he->globalPos(), hit.tooltip, this, hit.rect);
                return true;
            }
        }
        QToolTip::hideText();
        e->ignore();
        return true;
    }
    return QWidget::event(e);
}
