#pragma once
#include <QColor>
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

} // namespace Theme
