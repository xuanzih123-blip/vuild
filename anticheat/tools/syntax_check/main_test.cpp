// main_test.cpp - host side test for the portable core.
//
// Compiles the JSON reader and the hook table with a plain host compiler, so
// the logic can be verified without an iOS toolchain. It loads the generated
// hook table and checks the entries that matter.

#include <cstdio>
#include <string>
#include <vector>

#include "HookTable.h"
#include "TIZAntiCheat.h"

namespace {

int failures = 0;

void check(bool condition, const std::string& what) {
  std::printf("%-58s %s\n", what.c_str(), condition ? "ok" : "FAIL");
  if (!condition) ++failures;
}

}  // namespace

int main(int argc, char** argv) {
  std::string path = (argc > 1) ? argv[1] : "source/anticheat_hooks.json";

  tiz::HookTable table;
  std::string error;
  if (!table.loadFromFile(path, &error)) {
    std::printf("cannot load %s: %s\n", path.c_str(), error.c_str());
    return 2;
  }
  check(true, "hook table parses");

  const std::vector<tiz::HookEntry>& entries = table.entries();
  std::printf("entries: %zu\n", entries.size());
  check(entries.size() >= 10, "at least 10 hook entries");

  bool found_is_emulator = false;
  bool found_anti_hack_gate = false;
  bool found_report = false;
  bool found_mem_interval = false;
  bool tokens_ok = true;
  for (const tiz::HookEntry& e : entries) {
    std::printf("  %-34s %-34s %-18s token 0x%08x args %d\n",
                e.type_name.c_str(), e.method_name.c_str(),
                tiz::actionName(e.action), e.token, e.arg_count);
    if (e.token == 0) tokens_ok = false;
    if (e.type_name == "COW.GameFacade" && e.method_name == "IsEmulator" &&
        e.action == tiz::HookAction::ReturnFalse) {
      found_is_emulator = true;
    }
    if (e.type_name == "COW.GameFacade" &&
        e.method_name == "NeedffantihackEmulatorCheck") {
      found_anti_hack_gate = true;
    }
    if (e.type_name == "COW.EventLogger" && e.method_name == "LogReportCheat" &&
        e.action == tiz::HookAction::Skip) {
      found_report = true;
    }
    if (e.type_name == "COW.GameVarDef" &&
        e.method_name == "MonoMemCheckFrameInterval" &&
        e.action == tiz::HookAction::SetStaticInt) {
      found_mem_interval = true;
    }
  }
  check(found_is_emulator, "GameFacade.IsEmulator -> return_false");
  check(found_anti_hack_gate, "GameFacade.NeedffantihackEmulatorCheck present");
  check(found_report, "EventLogger.LogReportCheat -> skip");
  check(found_mem_interval, "GameVarDef.MonoMemCheckFrameInterval -> set_static_int");
  check(tokens_ok, "every entry carries a token");

  // profile defaults come from the document
  check(table.profile().method_info_method_pointer == 0,
        "method_info.method_pointer_offset loaded");
  check(table.profile().class_static_fields == 0xB8,
        "class_info.static_fields_offset loaded");

  // a broken document must be rejected, not silently accepted
  tiz::HookTable broken;
  std::string broken_error;
  bool broken_ok = broken.loadFromString("{\"hooks\":[{\"type\":\"a\"}]}", &broken_error);
  check(!broken_ok, "malformed hook entry is rejected");
  check(!broken_error.empty(), "rejection carries a reason");

  std::printf(failures ? "\n%d check(s) failed\n" : "\nall checks passed\n",
              failures);
  return failures ? 1 : 0;
}
