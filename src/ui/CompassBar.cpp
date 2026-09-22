#include "ui/CompassBar.h"
#include "ui/Theme.h"
#include <QPainter>
#include <QPolygon>
#include <QFile>
#include <QSvgRenderer>
#include <cmath>

static constexpr double kPxPerDeg = 10.0;

static const char* cardinalName(int deg) {
    switch (deg) {
        case   0: return "N";
        case  45: return "NE";
        case  90: return "E";
        case 135: return "SE";
        case 180: return "S";
        case 225: return "SW";
        case 270: return "W";
        case 315: return "NW";
        default:  return nullptr;
    }
}

CompassBar::CompassBar(QWidget* parent)
    : QWidget(parent)
{
    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_NoSystemBackground);
    setFixedHeight(44);

    QFile f(":/icons/navigation-off.svg");
    if (f.open(QIODevice::ReadOnly)) {
        m_navOffSvg = f.readAll();
    }
}

void CompassBar::setHeading(float degrees, bool valid) {
    m_heading = degrees;
    m_valid   = valid;
    update();
}

void CompassBar::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    const int W  = width();
    const int H  = height();
    const int cx = W / 2;

    // Semi-transparent background strip
    p.setPen(Qt::NoPen);
    p.setBrush(Theme::hudStripBackground);
    p.drawRect(0, 0, W, H);

    // Center marker: downward triangle at top-center
    QColor markerColor = Theme::accent;
    markerColor.setAlpha(220);
    p.setBrush(markerColor);
    QPolygon tri;
    tri << QPoint(cx - 5, 0) << QPoint(cx + 5, 0) << QPoint(cx, 9);
    p.drawPolygon(tri);

    if (!m_valid) {
        // No GPS fix / can't resolve azimuth -- crossed-out navigation icon
        // instead of a numeric heading.
        constexpr int kIconSize = 22;
        const qreal dpr = devicePixelRatioF();
        const int pixelSize = qRound(kIconSize * dpr);
        if (!m_navOffSvg.isEmpty() &&
            (m_navOffPixmap.isNull() || m_navOffPixmap.width() != pixelSize)) {
            QByteArray svg = m_navOffSvg;
            svg.replace("currentColor", Theme::textMuted.name(QColor::HexRgb).toUtf8());
            QPixmap pm(pixelSize, pixelSize);
            pm.fill(Qt::transparent);
            QSvgRenderer renderer(svg);
            QPainter sp(&pm);
            sp.setRenderHint(QPainter::Antialiasing);
            renderer.render(&sp, QRectF(0, 0, pixelSize, pixelSize));
            sp.end();
            pm.setDevicePixelRatio(dpr);
            m_navOffPixmap = pm;
        }
        if (!m_navOffPixmap.isNull()) {
            QRect box((W - kIconSize) / 2, (H - kIconSize) / 2, kIconSize, kIconSize);
            p.drawPixmap(box, m_navOffPixmap);
        }
        return;
    }

    QFont f = font();
    f.setPointSize(qMax(7, f.pointSize() - 1));
    f.setBold(true);
    p.setFont(f);
    QFontMetrics fm(f);

    QColor tickColor = Theme::textPrimary;
    tickColor.setAlpha(210);
    const int tickBase  = H - 1;
    const int majorH    = 14; // labeled ticks (cardinal/degree text) stay taller
    const int minorH    = 9;  // all unlabeled ticks share this height
    const int labelCy   = (9 + (tickBase - majorH)) / 2;  // midpoint between triangle apex and major tick top

    double halfRange = W / (2.0 * kPxPerDeg);
    int iLeft  = (int)std::floor(m_heading - halfRange);
    int iRight = (int)std::ceil (m_heading + halfRange);

    for (int d = iLeft; d <= iRight; ++d) {
        int x = cx + (int)std::round((d - m_heading) * kPxPerDeg);
        if (x < -20 || x >= W + 20) continue;

        int dn = ((d % 360) + 360) % 360;

        p.setPen(QPen(tickColor, 2));

        if (d % 10 == 0) {
            // Major tick + label
            p.drawLine(x, tickBase - majorH, x, tickBase);

            const char* cardinal = cardinalName(dn);
            QString label = cardinal ? QString(cardinal) : QString::number(dn);
            int tw = fm.horizontalAdvance(label);
            int th = fm.height();
            QRect tr(x - tw / 2, labelCy - th / 2, tw, th);
            p.drawText(tr, Qt::AlignCenter, label);
        } else if (d % 2 == 0) {
            p.drawLine(x, tickBase - minorH, x, tickBase);
        }
    }
}
