#pragma once
#include "config/AppConfig.h"
#include "serial/SerialPort.h"
#include "input/IInputSource.h"
#include "crsf/CrsfTypes.h"
#include <atomic>
#include <memory>

class App {
public:
    explicit App(AppConfig config);
    int run();
    void requestStop();

private:
    void mainLoop();
    RcChannels mapStateToChannels(const ControlState& state) const;
    void applyFailsafe(ControlState& state);

    AppConfig                     m_config;
    SerialPort                    m_serial;
    std::unique_ptr<IInputSource> m_input;
    std::atomic<bool>             m_running{true};
    bool                          m_inFailsafe = false;
};
