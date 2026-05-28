#pragma once
#include <QWidget>

class IPanel : public QWidget {
public:
    virtual void refresh() = 0;
protected:
    explicit IPanel(QWidget* parent = nullptr) : QWidget(parent) {}
};
