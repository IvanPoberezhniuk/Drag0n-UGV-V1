#pragma once
#include <QComboBox>

// A QComboBox whose popup list is a set of checkboxes rather than a single
// selection: clicking an item toggles it and keeps the popup open. Used to
// collapse a row of independent filter checkboxes into one control.
class CheckableComboBox : public QComboBox {
    Q_OBJECT
public:
    explicit CheckableComboBox(QWidget* parent = nullptr);

    // Fixed label shown while closed (e.g. "Levels"), instead of a
    // comma-joined summary of the checked items.
    void setTitleText(const QString& title);

    void addCheckableItem(const QString& text, bool checked = true);
    bool isChecked(int index) const;
    void setChecked(int index, bool checked);

signals:
    void checkedChanged();

protected:
    bool eventFilter(QObject* obj, QEvent* event) override;

private:
    void updateDisplayText();
    void toggleAt(const QModelIndex& index);

    QString m_titleText;
};
