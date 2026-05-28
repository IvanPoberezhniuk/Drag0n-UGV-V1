#pragma once
#include <QWidget>
#include "core/AppState.h"
#include "io/SerialWorker.h"

class QComboBox;
class QLineEdit;
class QPushButton;
class QLabel;

class ConnectionPanel : public QWidget {
public:
    explicit ConnectionPanel(AppState& state, SerialWorker& worker, QWidget* parent = nullptr);
    void refresh();

private:
    void onConnectClicked();
    void onRefreshClicked();

    AppState&     m_state;
    SerialWorker& m_worker;

    QLabel*      m_statusLabel  = nullptr;
    QComboBox*   m_portCombo    = nullptr;
    QLineEdit*   m_baudEdit     = nullptr;
    QPushButton* m_connectBtn   = nullptr;
    QLabel*      m_portInfoLabel = nullptr;
    QLabel*      m_baudInfoLabel = nullptr;
    QLabel*      m_pktLabel     = nullptr;
};
