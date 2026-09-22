#pragma once

#include "ui/IPanel.h"

class AppState;

// Local-only editor for the camera controls that will eventually be sent to
// the Raspberry Pi. The widgets intentionally do not alter VideoWorker or the
// live MediaMTX stream yet.
class CameraSettingsPanel : public IPanel {
public:
    explicit CameraSettingsPanel(AppState& state, QWidget* parent = nullptr);

    void refresh() override;
    QSize sizeHint() const override;

private:
    QWidget* buildEssentialPage();
    QWidget* buildAdvancedPage();

    AppState& m_state;
};
