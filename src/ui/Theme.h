#pragma once
#include <QColor>
#include <QPalette>
#include <QString>

namespace Theme {

static const QColor textMuted     { 136, 136, 136 };
static const QColor textDim       { 153, 153, 153 };
static const QColor textPrimary   { 229, 229, 229 };
static const QColor successGreen  {   0, 230,  51 };
static const QColor warningYellow { 255, 204,   0 };
static const QColor errorRed      { 255,  68,  68 };
static const QColor connectedGreen{   0, 200,  80 };

inline QString colorSS(QColor c) { return "color: " + c.name() + ";"; }

inline QPalette darkPalette() {
    QPalette p;
    p.setColor(QPalette::Window,          QColor(45,  45,  45));
    p.setColor(QPalette::WindowText,      Qt::white);
    p.setColor(QPalette::Base,            QColor(30,  30,  30));
    p.setColor(QPalette::AlternateBase,   QColor(53,  53,  53));
    p.setColor(QPalette::ToolTipBase,     Qt::white);
    p.setColor(QPalette::ToolTipText,     Qt::white);
    p.setColor(QPalette::Text,            Qt::white);
    p.setColor(QPalette::Button,          QColor(53,  53,  53));
    p.setColor(QPalette::ButtonText,      Qt::white);
    p.setColor(QPalette::BrightText,      Qt::red);
    p.setColor(QPalette::Highlight,       QColor(42, 130, 218));
    p.setColor(QPalette::HighlightedText, Qt::black);
    p.setColor(QPalette::Link,            QColor(42, 130, 218));
    return p;
}

} // namespace Theme
