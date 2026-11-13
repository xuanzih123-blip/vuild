// AntiBan.mm - bootstrap of the anti ban layer.
//
// Order of business when the library is loaded into the game:
//
//   1. clean the environment, deny debugger attach, remove the traces
//   2. wait for the game image and bind the IL2CPP runtime
//   3. wait until the runtime is initialised
//   4. install the managed hooks
//   5. keep them in place, a hotfix can put the original code back
//
// The native implementation of the game is never modified: the pointer inside
// the method info is redirected, so the code pages of the game stay byte
// identical and a checksum over them still matches.
#import <Foundation/Foundation.h>

#include <pthread.h>
#include <string.h>
#include <unistd.h>

#include "Hooks.h"
#include "Il2CppApi.h"
#include "Sanitizer.h"

namespace mizze {
namespace {

void* InstallThread(void* unused) {
  (void)unused;
  @autoreleasepool {
    sanitize::DenyDebuggerAttach();
    sanitize::CleanEnvironment();
    sanitize::RemoveTraces();
    sanitize::SealInheritedDescriptors();

    void* image = nullptr;
    size_t image_size = 0;
    std::string image_name;
    for (int attempt = 0; attempt < 120 && image == nullptr; ++attempt) {
      image = findGameImage(&image_size, &image_name);
      if (image == nullptr) usleep(500 * 1000);
    }
    if (image == nullptr) {
      NSLog(@"[Mizze] no game image, the anti ban layer stays off");
      return nullptr;
    }

    Il2CppApi api;
    if (!api.bind(image)) {
      NSLog(@"[Mizze] the game does not export the il2cpp runtime, "
            @"the anti ban layer stays off");
      return nullptr;
    }

    for (int attempt = 0; attempt < 120 && !api.runtimeReady(); ++attempt) {
      usleep(500 * 1000);
    }
    NSLog(@"[Mizze] runtime %@", api.runtimeReady() ? @"ready" : @"not ready");

    HookStats stats = installHooks(api);
    NSLog(@"[Mizze] hooks installed %d, failed %d", stats.installed, stats.failed);

    while (true) {
      usleep(2 * 1000 * 1000);
      int restored = reapplyHooks(api);
      if (restored > 0) {
        NSLog(@"[Mizze] re-applied %d hooks after a hotfix", restored);
      }
    }
  }
  return nullptr;
}

}  // namespace
}  // namespace mizze

__attribute__((constructor)) static void MizzeAntiBanBootstrap() {
  pthread_t thread;
  if (pthread_create(&thread, nullptr, &mizze::InstallThread, nullptr) == 0) {
    pthread_detach(thread);
  }
}
