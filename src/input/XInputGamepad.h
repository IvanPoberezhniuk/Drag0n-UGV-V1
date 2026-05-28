#pragma once
#include "input/IInputSource.h"
#include "input/EdgeDetector.h"
#include "config/AppConfig.h"

class XInputGamepad : public IInputSource {
public:
    explicit XInputGamepad(int playerIndex, const AppConfig::InputCfg& cfg);
    InputFrame  poll()        override;
    bool        isConnected() const override;
    const char* name()        const override;

private:
    int  m_index;
    bool m_connected  = false;
    bool m_armed      = false;
    int  m_driveMode  = 1;
    std::array<EdgeDetector, 4> m_btnEdge;  // A=0, B=1, Y=2, X=3
    const AppConfig::InputCfg& m_cfg;
};
