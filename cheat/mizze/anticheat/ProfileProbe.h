// ProfileProbe.h - reads the offset profile out of the running game.
#pragma once

#include "Il2CppApi.h"

namespace mizze {
namespace probe {

// Writes the offsets of the interesting classes into the device log.
void logProfile(const Il2CppApi& api);

}  // namespace probe
}  // namespace mizze
