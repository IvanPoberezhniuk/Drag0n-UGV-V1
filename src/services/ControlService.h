#pragma once
#include "core/ControlState.h"
#include "crsf/CrsfTypes.h"
#include "config/AppConfig.h"

namespace ChannelMapper {

RcChannels mapChannels(const ControlState& ctrl,
                       const AppConfig::ChannelsCfg& channels);

} // namespace ChannelMapper
