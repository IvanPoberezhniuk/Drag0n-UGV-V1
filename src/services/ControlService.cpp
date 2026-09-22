#include "services/ControlService.h"
#include <algorithm>

namespace ChannelMapper {

static uint16_t mapAxis(float v) {
    float c = std::max(-1.0f, std::min(1.0f, v));
    if (c >= 0.0f)
        return static_cast<uint16_t>(CH_CENTER + c * (CH_MAX - CH_CENTER));
    return static_cast<uint16_t>(CH_CENTER + c * (CH_CENTER - CH_MIN));
}

RcChannels mapChannels(const ControlState& ctrl,
                       const AppConfig::ChannelsCfg& ch) {
    RcChannels rc{};
    for (auto& c : rc.ch) c = CH_CENTER;

    rc.ch[ch.steering - 1] = mapAxis(ctrl.steering);
    rc.ch[ch.throttle - 1] = mapAxis(ctrl.throttle);

    switch (ctrl.driveMode) {
        case DriveMode::TwoWD:  rc.ch[ch.mode - 1] = CH_MIN;    break;
        case DriveMode::FourWD: rc.ch[ch.mode - 1] = CH_CENTER; break;
        case DriveMode::SixWD:  rc.ch[ch.mode - 1] = CH_MAX;    break;
    }

    rc.ch[ch.lights - 1] = ctrl.lightsOn ? CH_MAX : CH_MIN;
    rc.ch[ch.arm    - 1] = ctrl.armed    ? CH_MAX : CH_MIN;
    rc.ch[ch.estop  - 1] = ctrl.estop    ? CH_MAX : CH_MIN;
    return rc;
}

} // namespace ChannelMapper
