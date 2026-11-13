// SdkBlock.mm - keeps the native anti cheat SDK from being loaded at all.
//
// The managed gates (GameFacade.NeedffantihackEmulatorCheck and friends) stop
// the SDK from running, but the native library itself is still pulled in by a
// P/Invoke. Interposing dlopen keeps it out of the process: the SDK can not
// report anything if it was never mapped.
//
// The interpose only fires for libraries whose name contains one of the
// tokens below, so everything else loads normally. Every block is logged so
// the device log shows exactly what was refused.
#import <Foundation/Foundation.h>

#include <dlfcn.h>

#include <cstdint>
#include <cstring>
#include <string>

namespace tiz {
namespace {

// Name fragments of the native anti cheat / emulator detection libraries.
const char* const kBlockedTokens[] = {
    "tersafe",    // Tencent TerSafe
    "tp2_",       // Tencent protect
    "libano",     // the Ano SDK family
    "anosdk",
    "ffantihack", // the FF anti hack bridge
    "antihack",
    "anticheat",
};

bool IsBlockedPath(const char* path) {
  if (!path) return false;
  std::string s(path);
  for (const char* token : kBlockedTokens) {
    if (s.find(token) != std::string::npos) return true;
  }
  return false;
}

}  // namespace

// The replacement for dlopen.
extern "C" void* TIZAntiCheatDlOpen(const char* path, int mode) {
  if (IsBlockedPath(path)) {
    NSLog(@"[TIZAntiCheat] blocked native SDK load: %s", path);
    return nullptr;
  }
  return dlopen(path, mode);
}

}  // namespace tiz

// DYLD interpose: the replacement wins for every image loaded after this one.
__attribute__((used)) static struct {
  const void* replacement;
  const void* replacee;
} tiz_interpose_dlopen __attribute__((section("__DATA,__interpose"))) = {
    (const void*)(uintptr_t)&tiz::TIZAntiCheatDlOpen,
    (const void*)(uintptr_t)&dlopen,
};
