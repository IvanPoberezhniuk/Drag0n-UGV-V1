#include "ui/AppStyle.h"
#include <QIcon>

QIcon AppStyle::standardIcon(StandardPixmap standardIcon, const QStyleOption* option,
                              const QWidget* widget) const {
    switch (standardIcon) {
        case SP_TitleBarCloseButton:
            return QIcon(":/icons/x.svg");
        case SP_TitleBarNormalButton: // dock widget's float/undock button
            return QIcon(":/icons/square.svg");
        default:
            return QProxyStyle::standardIcon(standardIcon, option, widget);
    }
}
