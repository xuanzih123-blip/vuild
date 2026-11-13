// TIZAntiCheat.h - shared types for the anti-cheat neutralisation layer.
//
// The hook table is data driven: it is generated from the IL2CPP metadata and
// embedded in the dylib, so the same binary keeps working when the game
// renumbers tokens but keeps the managed names.
#ifndef TIZ_ANTICHEAT_H
#define TIZ_ANTICHEAT_H

#include <cstdint>
#include <string>
#include <vector>

namespace tiz {

enum class HookAction {
  ReturnFalse,       // detour returns false (w0 = 0) whatever the original did
  ReturnTrue,        // detour returns true  (w0 = 1)
  Skip,              // detour does nothing and returns immediately
  SetStaticInt,      // write an integer into a static field of the class
  SetStaticString,   // write a string into a static field of the class
};

const char* actionName(HookAction a);
bool actionFromName(const std::string& name, HookAction* out);

struct HookEntry {
  std::string type_name;      // managed class, e.g. "COW.GameFacade"
  std::string method_name;    // managed method, e.g. "IsEmulator"
  uint32_t token = 0;         // IL2CPP method/field token, 0 when unknown
  int arg_count = 0;           // argument count, 0 when unknown
  uint32_t field_offset = 0;   // runtime offset of a static field, 0 = unknown
  HookAction action = HookAction::Skip;
  int64_t int_value = 0;      // value for SetStaticInt
  std::string string_value;   // value for SetStaticString
  std::string note;           // free text, copied from the generator
};

// Outcome of one installation round.
struct InstallReport {
  int installed = 0;
  int skipped = 0;
  int failed = 0;
};

// Profile of the IL2CPP runtime layout. Defaults match Unity 2019-2021;
// the values are overridable from build.yml / the embedded JSON.
struct Il2CppProfile {
  uint32_t method_info_method_pointer = 0x0;
  uint32_t method_info_name = 0x10;
  uint32_t method_info_klass = 0x18;
  uint32_t method_info_token = 0x30;
  uint32_t class_static_fields = 0xb8;
  uint32_t class_name = 0x10;
  uint32_t class_namespace = 0x18;
  uint32_t field_info_offset = 0x0;
  uint32_t field_info_name = 0x8;
};

}  // namespace tiz

#endif  // TIZ_ANTICHEAT_H
