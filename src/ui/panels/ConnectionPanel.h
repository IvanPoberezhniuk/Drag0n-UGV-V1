#pragma once
#include "ui/IPanel.h"
#include "core/AppState.h"
#include "config/AppConfig.h"
#include "io/SerialWorker.h"

class QComboBox;
class QPushButton;
class QLabel;
class HoverInfoIcon;
class LiveDot;

class ConnectionPanel : public IPanel {
public:
    explicit ConnectionPanel(AppState& state, SerialWorker& worker,
                             const AppConfig& config, QWidget* parent = nullptr);
    void refresh() override;

private:
    void onConnectClicked();
    void refreshPortList();

    AppState&     m_state;
    SerialWorker& m_worker;

    QLabel*        m_statusLabel = nullptr;
    LiveDot*       m_statusDot   = nullptr;
    QComboBox*     m_portCombo   = nullptr;
    QPushButton*   m_connectBtn  = nullptr;
    HoverInfoIcon* m_infoIcon    = nullptr;
};
