#pragma once
#include <QProxyStyle>

// Fusion-based proxy style that swaps a handful of Qt's built-in standard
// icons for Tabler-icon equivalents (resources/icons/, MIT license), so
// QDockWidget title bars (float/close buttons) match the rest of the app's
// icon set instead of Fusion's default glyphs.
class AppStyle : public QProxyStyle {
public:
    using QProxyStyle::QProxyStyle;

    QIcon standardIcon(StandardPixmap standardIcon, const QStyleOption* option = nullptr,
                        const QWidget* widget = nullptr) const override;
};
