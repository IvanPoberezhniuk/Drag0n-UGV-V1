#pragma once
#include <QDialog>
#include <QFont>

class QListWidget;
class QStackedWidget;
class QFontComboBox;
class QSpinBox;
class QLabel;

class SettingsDialog : public QDialog {
    Q_OBJECT
public:
    explicit SettingsDialog(QWidget* parent = nullptr);

private slots:
    void onApply();
    void onOk();
    void onCancel();
    void updatePreview();

private:
    QWidget* buildUiPage();

    QListWidget*    m_sidebar   = nullptr;
    QStackedWidget* m_stack     = nullptr;
    QFontComboBox*  m_fontCombo = nullptr;
    QSpinBox*       m_fontSize  = nullptr;
    QLabel*         m_preview   = nullptr;

    QFont m_originalFont;
};
