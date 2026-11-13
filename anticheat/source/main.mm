// main.mm - entry point of the TIZAntiCheat dylib.
//
// The library is loaded into the game process. It waits for the IL2CPP
// runtime to come up, then installs the detours described by the generated
// hook table (embedded in the binary, so no file dependency at runtime).
#import <Foundation/Foundation.h>
#include <dispatch/dispatch.h>
#include <dlfcn.h>
#include <mach-o/dyld.h>
#include <mach-o/loader.h>
#include <pthread.h>
#include <string.h>
#include <unistd.h>

#include <string>
#include <vector>

#include "HookTable.h"
#include "Il2CppResolver.h"
#include "TIZAntiCheat.h"

#include "anticheat_hooks_embedded.h"

namespace tiz {

InstallReport installHooks(const HookTable& table, Il2CppResolver& resolver);

namespace {

const char* kGameImageHints[] = {"UnityFramework", "libil2cpp", "il2cpp",
                                 "GameAssembly", "main"};

// Walks the load commands to get the real mapped size of the image, so the
// pattern scanner never reads past the end of the mapping.
size_t ImageMappedSize(const struct mach_header* header) {
  size_t highest = 0;
  const uint8_t* cmd = reinterpret_cast<const uint8_t*>(header) +
                       sizeof(struct mach_header);
  for (uint32_t i = 0; i < header->ncmds; ++i) {
    const struct load_command* lc =
        reinterpret_cast<const struct load_command*>(cmd);
    if (lc->cmdsize == 0) break;
    if (lc->cmd == LC_SEGMENT_64) {
      const struct segment_command_64* seg =
          reinterpret_cast<const struct segment_command_64*>(lc);
      uintptr_t start = reinterpret_cast<uintptr_t>(seg->vmaddr) -
                        reinterpret_cast<uintptr_t>(header);
      uintptr_t end = start + seg->vmsize;
      if (end > highest) highest = end;
    }
    cmd += lc->cmdsize;
  }
  return highest;
}

void* FindGameImage(size_t* size_out, std::string* name_out) {
  uint32_t count = _dyld_image_count();
  for (uint32_t i = 0; i < count; ++i) {
    const char* raw = _dyld_get_image_name(i);
    if (!raw) continue;
    std::string name(raw);
    for (const char* hint : kGameImageHints) {
      if (name.find(hint) != std::string::npos) {
        const struct mach_header* header = _dyld_get_image_header(i);
        if (!header) continue;
        size_t size = ImageMappedSize(header);
        if (size == 0) continue;
        *size_out = size;
        *name_out = name;
        return const_cast<void*>(static_cast<const void*>(header));
      }
    }
  }
  return nullptr;
}

void* LoadHookTable(std::string* error) {
  HookTable table;
  if (!table.loadFromString(kAnticheatHooksJson, error)) return nullptr;
  // An on-disk table wins, so the behaviour can be changed without a rebuild.
  const char* override_path = "/var/mobile/Library/TIZAntiCheat/hooks.json";
  std::string file_error;
  HookTable file_table;
  if (file_table.loadFromFile(override_path, &file_error) &&
      !file_table.entries().empty()) {
    NSLog(@"[TIZAntiCheat] using hook table override %s",
          override_path);
    return new HookTable(file_table);
  }
  return new HookTable(table);
}

void* InstallThread(void* unused) {
  (void)unused;
  for (int attempt = 0; attempt < 120; ++attempt) {
    size_t size = 0;
    std::string name;
    void* base = FindGameImage(&size, &name);
    if (base) {
      std::string error;
      HookTable* table = static_cast<HookTable*>(LoadHookTable(&error));
      if (!table) {
        NSLog(@"[TIZAntiCheat] hook table rejected: %s", error.c_str());
        return nullptr;
      }
      Il2CppResolver resolver(table->profile());
      resolver.setGameImage(base, size, name);
      if (resolver.bindExportedApi(base)) {
        NSLog(@"[TIZAntiCheat] il2cpp API bound from %s", name.c_str());
      } else {
        NSLog(@"[TIZAntiCheat] no exported il2cpp API in %s, pattern mode only",
              name.c_str());
      }
      resolver.bindStringApi(base);
      InstallReport report = installHooks(*table, resolver);
      NSLog(@"[TIZAntiCheat] installed %d, skipped %d, failed %d",
            report.installed, report.skipped, report.failed);
      delete table;
      return nullptr;
    }
    usleep(500 * 1000);
  }
  NSLog(@"[TIZAntiCheat] game image never appeared, giving up");
  return nullptr;
}

__attribute__((constructor)) static void TIZAntiCheatBootstrap() {
  pthread_t thread;
  if (pthread_create(&thread, nullptr, InstallThread, nullptr) == 0) {
    pthread_detach(thread);
  } else {
    NSLog(@"[TIZAntiCheat] could not start the install thread");
  }
}

}  // namespace
}  // namespace tiz
