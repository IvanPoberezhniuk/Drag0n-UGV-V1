#pragma once
#include <QLabel>

// A static info-circle icon that shows an HudTooltip (see HudTooltip.h, not
// native QToolTip) on hover, built from whatever HTML the owner last set via
// setTooltipHtml(). Used to tuck a details block (e.g. connection stats)
// behind an icon instead of always-visible labels.
class HoverInfoIcon : public QLabel {
    Q_OBJECT
public:
    explicit HoverInfoIcon(QWidget* parent = nullptr);

    void setTooltipHtml(const QString& html);

protected:
    void enterEvent(QEnterEvent* e) override;
    void leaveEvent(QEvent* e) override;

private:
    QString m_html;
};
