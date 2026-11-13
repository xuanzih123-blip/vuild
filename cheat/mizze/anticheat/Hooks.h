// Hooks.h - the managed hook table and the installer.
#pragma once

#include <cstdint>

#include "Il2CppApi.h"

namespace mizze {

enum class Action {
  ReturnFalse,    // the managed method answers false
  ReturnTrue,     // the managed method answers true
  Skip,           // the managed method does nothing
  SetStaticInt,   // write an integer into a static field
  SetStaticString,// write a managed string into a static field
};

struct HookSpec {
  const char* type;    // managed class, "COW.GameFacade"
  const char* method;  // managed method or managed field
  Action action;
  int arg_count;       // argument count, disambiguates overloads
  const char* note;
  int64_t int_value;       // value for SetStaticInt
  const char* string_value; // value for SetStaticString
};

struct HookStats {
  int installed = 0;
  int skipped = 0;
  int failed = 0;
};

// Installs every entry of the table. Methods that cannot be resolved yet are
// retried, because the managed runtime finishes loading its metadata after
// the game image shows up.
HookStats installHooks(const Il2CppApi& api);

// Rewrites the method pointers again. A hotfix can put the original
// implementation back while the game runs.
int reapplyHooks(const Il2CppApi& api);

int hookSpecCount();
const HookSpec& hookSpecAt(int index);

}  // namespace mizze
