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
    int  m_driveMode  = 1;
    int  m_turnMode   = 1;
    std::array<EdgeDetector, 5> m_btnEdge;  // A=0, B=1, Y=2, X=3, LB=4
    const AppConfig::InputCfg& m_cfg;
};
