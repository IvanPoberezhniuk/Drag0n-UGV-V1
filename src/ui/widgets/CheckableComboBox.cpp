#include "ui/widgets/CheckableComboBox.h"
#include <QStandardItemModel>
#include <QLineEdit>
#include <QMouseEvent>
#include <QAbstractItemView>

CheckableComboBox::CheckableComboBox(QWidget* parent)
    : QComboBox(parent)
{
    setModel(new QStandardItemModel(this));

    // Editable + read-only line edit is the simplest way to show a custom
    // summary string instead of the (single) current item's text.
    setEditable(true);
    lineEdit()->setReadOnly(true);
    lineEdit()->setCursor(Qt::PointingHandCursor);
    lineEdit()->installEventFilter(this);

    view()->viewport()->installEventFilter(this);
}

void CheckableComboBox::setTitleText(const QString& title) {
    m_titleText = title;
    updateDisplayText();
}

void CheckableComboBox::addCheckableItem(const QString& text, bool checked) {
    auto* item = new QStandardItem(text);
    item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
    item->setFlags(item->flags() & ~Qt::ItemIsSelectable);
    item->setCheckState(checked ? Qt::Checked : Qt::Unchecked);
    static_cast<QStandardItemModel*>(model())->appendRow(item);
    updateDisplayText();
}

bool CheckableComboBox::isChecked(int index) const {
    return model()->index(index, 0).data(Qt::CheckStateRole).toInt() == Qt::Checked;
}

void CheckableComboBox::setChecked(int index, bool checked) {
    model()->setData(model()->index(index, 0), checked ? Qt::Checked : Qt::Unchecked, Qt::CheckStateRole);
    updateDisplayText();
}

void CheckableComboBox::toggleAt(const QModelIndex& index) {
    if (!index.isValid()) return;
    const bool checked = index.data(Qt::CheckStateRole).toInt() == Qt::Checked;
    model()->setData(index, checked ? Qt::Unchecked : Qt::Checked, Qt::CheckStateRole);
    updateDisplayText();
    emit checkedChanged();
}

void CheckableComboBox::updateDisplayText() {
    if (!m_titleText.isEmpty()) {
        lineEdit()->setText(m_titleText);
        return;
    }
    QStringList checked;
    for (int i = 0; i < model()->rowCount(); ++i) {
        if (isChecked(i)) checked << model()->index(i, 0).data(Qt::DisplayRole).toString();
    }
    lineEdit()->setText(checked.isEmpty() ? "None" : checked.join(", "));
}

bool CheckableComboBox::eventFilter(QObject* obj, QEvent* event) {
    if (obj == lineEdit() && event->type() == QEvent::MouseButtonPress) {
        showPopup();
        return true;
    }
    if (obj == view()->viewport() &&
        (event->type() == QEvent::MouseButtonPress || event->type() == QEvent::MouseButtonRelease)) {
        // Swallow both press and release so QComboBox's default
        // click-to-close/select behavior never runs; toggle only on
        // release, matching normal checkbox click feel.
        if (event->type() == QEvent::MouseButtonRelease) {
            auto* me = static_cast<QMouseEvent*>(event);
            toggleAt(view()->indexAt(me->pos()));
        }
        return true;
    }
    return QComboBox::eventFilter(obj, event);
}
