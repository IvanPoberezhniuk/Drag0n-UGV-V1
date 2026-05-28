#include "services/ControlService.h"
#include <algorithm>

namespace DroneControlService {

static uint16_t mapAxis(float v) {
    float c = std::max(-1.0f, std::min(1.0f, v));
    if (c >= 0.0f)
        return static_cast<uint16_t>(CH_CENTER + c * (CH_MAX - CH_CENTER));
    return static_cast<uint16_t>(CH_CENTER + c * (CH_CENTER - CH_MIN));
}

RcChannels mapChannels(const ControlState& ctrl, const AppConfig::ChannelsCfg& ch) {
    RcChannels rc{};
    for (auto& c : rc.ch) c = CH_CENTER;

    rc.ch[ch.steering - 1] = mapAxis(ctrl.steering);
    rc.ch[ch.throttle - 1] = mapAxis(ctrl.throttle);

    if (ctrl.driveMode == 1)      rc.ch[ch.mode - 1] = CH_MIN;
    else if (ctrl.driveMode == 2) rc.ch[ch.mode - 1] = CH_CENTER;
    else                          rc.ch[ch.mode - 1] = CH_MAX;

    rc.ch[ch.lights - 1] = ctrl.lightsOn ? CH_MAX : CH_MIN;
    rc.ch[ch.arm    - 1] = ctrl.armed    ? CH_MAX : CH_MIN;
    rc.ch[ch.estop  - 1] = ctrl.estop    ? CH_MAX : CH_MIN;

    return rc;
}

} // namespace DroneControlService
