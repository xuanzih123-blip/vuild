// ProfileProbe.mm - reads the real offset profile out of the running game.
//
// The menu carries a profile with hard coded field offsets. A profile belongs
// to one build of the game, so instead of trusting it, the running game is
// asked where its fields actually live: every managed field of the classes the
// menu reads is resolved through the runtime and the interesting offsets are
// written to the log.
//
// One run of the game is enough to see whether the profile still fits.
#import <Foundation/Foundation.h>

#include <cstdint>
#include <cstdio>
#include <string>

#include "Il2CppApi.h"

namespace mizze {
namespace probe {
namespace {

// The offsets the menu profile and the field table of the game use. Only
// fields that sit on one of them are printed, so the log stays short.
const int kInterestingOffsets[] = {
    // profile of the menu
    0x260, 0x268, 0x2D0, 0x3E8, 0x408, 0x490, 0x4A0, 0x4D8, 0x614, 0x628,
    0x690, 0x6A0, 0x6A8, 0x6C0, 0x6C8, 0x6D0, 0x6D8, 0x6E0, 0x6F0, 0x6F8,
    0x718, 0x720, 0x728, 0x730, 0x740, 0x760, 0x768, 0x770, 0xA50, 0x1258,
    0x1D48,
    // field table of the game
    0x2B0, 0x358, 0x434, 0x498, 0x4A8, 0x550, 0x5A0, 0x7C, 0xD0, 0x10C,
    0x270,
};

const char* const kClasses[] = {
    "COW.GamePlay.Player",
    "COW.GamePlay.PlayerAttributes",
    "COW.GamePlay.PlayerNetwork",
    "COW.GameFacade",
    "COW.MatchGame",
    "COW.GameVarDef",
};

bool IsInteresting(int offset) {
  for (int value : kInterestingOffsets) {
    if (value == offset) return true;
  }
  return false;
}

void Visitor(const char* declaring_class, const char* field_name, int offset,
             void* context) {
  const bool* log_all = static_cast<const bool*>(context);
  if (log_all && !*log_all && !IsInteresting(offset)) return;
  NSLog(@"[Mizze] profile %s.%s @%#x", declaring_class, field_name, offset);
}

void* ClassByName(const Il2CppApi& api, const char* type) {
  std::string name(type);
  size_t dot = name.find('.');
  if (dot == std::string::npos) return api.classFromName("", name.c_str());
  return api.classFromName(name.substr(0, dot).c_str(), name.substr(dot + 1).c_str());
}

}  // namespace

// Writes the profile of the interesting classes into the device log.
void logProfile(const Il2CppApi& api) {
  for (const char* type : kClasses) {
    void* klass = ClassByName(api, type);
    if (!klass) {
      NSLog(@"[Mizze] profile class %s not found", type);
      continue;
    }
    NSLog(@"[Mizze] profile class %s size %d", type,
          api.classInstanceSize(klass));
    // The class with the static fields is small, so every one of them is
    // printed. The big classes only print the offsets that matter.
    bool log_all = std::string(type) == "COW.GameVarDef";
    api.forEachField(klass, &Visitor, &log_all);
  }
}

}  // namespace probe
}  // namespace mizze
