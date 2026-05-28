#pragma once
#include <QAbstractButton>

class ToggleSwitch : public QAbstractButton {
public:
    explicit ToggleSwitch(const QString& label, QWidget* parent = nullptr);
    QSize sizeHint() const override;
protected:
    void paintEvent(QPaintEvent*) override;
private:
    QString m_label;
};
