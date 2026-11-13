// Hooks.mm - installs the neutralisation detours.
//
// Preferred mechanism: rewrite Il2CppMethodInfo::methodPointer so the game
// calls our detour. That needs no Substrate and is atomic.
//
// Fallback (pattern scanned addresses only): MSHookFunction from Substrate or
// ElleKit, when the device has it.
#import <Foundation/Foundation.h>

#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

#include "HookTable.h"
#include "Il2CppResolver.h"

extern "C" void MSHookFunction(void* symbol, void* replacement, void** result)
    __attribute__((weak));

namespace tiz {
namespace {

// The detours ignore the original arguments on purpose: they replace the
// call, so the argument registers are simply not consumed.
bool DetourReturnFalse(void* self) {
  (void)self;
  return false;
}

bool DetourReturnTrue(void* self) {
  (void)self;
  return true;
}

void DetourSkip(void* self) {
  (void)self;
}

void* DetourFor(HookAction action) {
  switch (action) {
    case HookAction::ReturnFalse: return reinterpret_cast<void*>(&DetourReturnFalse);
    case HookAction::ReturnTrue: return reinterpret_cast<void*>(&DetourReturnTrue);
    case HookAction::Skip: return reinterpret_cast<void*>(&DetourSkip);
    default: return nullptr;
  }
}

int ArgCountFor(const HookEntry& e) {
  // 0 means "unknown": the resolver then tries the overload list.
  return e.arg_count;
}

bool WriteStaticInt(Il2CppResolver& resolver, const HookEntry& e) {
  // The runtime offset of a static field is only known when the table carries
  // it. Writing a guessed offset would corrupt whatever sits at that slot,
  // so an unknown offset is skipped instead.
  if (e.field_offset == 0) return false;
  ResolvedField f = resolver.resolveStaticField(e.type_name, e.method_name,
                                                e.field_offset);
  if (!f.valid()) return false;
  *static_cast<int64_t*>(f.static_address) = e.int_value;
  return true;
}

bool WriteStaticString(Il2CppResolver& resolver, const HookEntry& e) {
  if (e.field_offset == 0) return false;
  ResolvedField f = resolver.resolveStaticField(e.type_name, e.method_name,
                                                e.field_offset);
  if (!f.valid()) return false;
  void* managed = resolver.newManagedString(e.string_value);
  if (!managed) return false;
  *reinterpret_cast<void**>(f.static_address) = managed;
  return true;
}

}  // namespace

// Installs every entry of the table. Returns a short report for the log.
InstallReport installHooks(const HookTable& table, Il2CppResolver& resolver) {
  InstallReport report;
  for (const HookEntry& e : table.entries()) {
    if (e.action == HookAction::SetStaticInt ||
        e.action == HookAction::SetStaticString) {
      bool ok = (e.action == HookAction::SetStaticInt)
                    ? WriteStaticInt(resolver, e)
                    : WriteStaticString(resolver, e);
      if (ok) {
        report.installed++;
      } else {
        report.skipped++;
        NSLog(@"[TIZAntiCheat] static %s.%s not applied (no runtime offset yet)",
              e.type_name.c_str(), e.method_name.c_str());
      }
      continue;
    }

    void* detour = DetourFor(e.action);
    if (!detour) {
      report.skipped++;
      continue;
    }

    ResolvedMethod m = resolver.resolveMethod(e.type_name, e.method_name,
                                              ArgCountFor(e));
    if (!m.valid()) {
      report.failed++;
      NSLog(@"[TIZAntiCheat] unresolved %s.%s", e.type_name.c_str(),
            e.method_name.c_str());
      continue;
    }

    if (m.method_info) {
      // rewrite the method pointer in place
      if (resolver.setMethodPointer(m.method_info, detour)) {
        report.installed++;
        NSLog(@"[TIZAntiCheat] hooked %s.%s (method info %p)", e.type_name.c_str(),
              e.method_name.c_str(), m.method_info);
      } else {
        report.failed++;
      }
      continue;
    }

    if (MSHookFunction) {
      void* original = nullptr;
      MSHookFunction(m.address, detour, &original);
      report.installed++;
      NSLog(@"[TIZAntiCheat] hooked %s.%s (pattern, %p)", e.type_name.c_str(),
            e.method_name.c_str(), m.address);
      continue;
    }

    report.failed++;
    NSLog(@"[TIZAntiCheat] no way to patch %s.%s (%p)", e.type_name.c_str(),
          e.method_name.c_str(), m.address);
  }
  return report;
}

}  // namespace tiz
