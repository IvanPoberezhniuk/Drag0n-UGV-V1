#pragma once
#include "core/ControlState.h"
#include "crsf/CrsfTypes.h"
#include "config/AppConfig.h"

namespace DroneControlService {

// Maps a ControlState to CRSF RcChannels using the channel mapping from config.
RcChannels mapChannels(const ControlState& ctrl, const AppConfig::ChannelsCfg& channels);

} // namespace DroneControlService
