#pragma once
#include "core/AppState.h"
#include <QDialog>
#include <QFont>

class QListWidget;
class QStackedWidget;
class QFontComboBox;
class QSpinBox;
class QSlider;
class QLabel;

class SettingsDialog : public QDialog {
    Q_OBJECT
public:
    explicit SettingsDialog(AppState& state, QWidget* parent = nullptr);

private slots:
    void onApply();
    void onOk();
    void onCancel();
    void updatePreview();

private:
    QWidget* buildUiPage();

    AppState&       m_appState;
    QListWidget*    m_sidebar      = nullptr;
    QStackedWidget* m_stack        = nullptr;
    QFontComboBox*  m_fontCombo    = nullptr;
    QSpinBox*       m_fontSize     = nullptr;
    QLabel*         m_preview      = nullptr;
    QSlider*        m_wheelSlider  = nullptr;
    QSpinBox*       m_wheelSize    = nullptr;

    QFont m_originalFont;
    int   m_originalWheelSize = 100;
};
