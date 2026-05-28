#pragma once
#include "ui/IPanel.h"
#include "core/AppState.h"
#include "config/AppConfig.h"
#include "io/SerialWorker.h"

class QComboBox;
class QLineEdit;
class QPushButton;
class QLabel;

class ConnectionPanel : public IPanel {
public:
    explicit ConnectionPanel(AppState& state, SerialWorker& worker,
                             const AppConfig& config, QWidget* parent = nullptr);
    void refresh() override;

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
