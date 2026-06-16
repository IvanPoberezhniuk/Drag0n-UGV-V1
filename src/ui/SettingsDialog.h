#pragma once
#include "core/AppState.h"
#include "input/KeyBindings.h"
#include <QDialog>
#include <QFont>

class QListWidget;
class QStackedWidget;
class QFontComboBox;
class QSpinBox;
class QSlider;
class QLabel;
class QTableWidget;

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
    QWidget* buildControlsPage();
    void     populateBindingsTable();

    AppState&       m_appState;
    QListWidget*    m_sidebar      = nullptr;
    QStackedWidget* m_stack        = nullptr;

    // UI page
    QFontComboBox*  m_fontCombo    = nullptr;
    QSpinBox*       m_fontSize     = nullptr;
    QLabel*         m_preview      = nullptr;
    QSlider*        m_wheelSlider  = nullptr;
    QSpinBox*       m_wheelSize    = nullptr;

    // Controls page
    QTableWidget*   m_bindingsTable = nullptr;

    QFont       m_originalFont;
    int         m_originalWheelSize = 100;
    KeyBindings m_editedBindings;
    KeyBindings m_originalBindings;
};
