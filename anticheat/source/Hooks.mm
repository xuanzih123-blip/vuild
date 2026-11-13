// Hooks.mm - installs the neutralisation detours.
//
// Preferred mechanism: rewrite Il2CppMethodInfo::methodPointer so the game
// calls our detour. That needs no Substrate and is atomic.
//
// Fallback (pattern scanned addresses only): MSHookFunction from Substrate or
// ElleKit, when the device has it.
#import <Foundation/Foundation.h>

#include <dlfcn.h>
#include <unistd.h>

#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

#include "HookTable.h"
#include "Il2CppResolver.h"

namespace tiz {
namespace {

// MSHookFunction comes from Substrate / ElleKit, which is not linked against
// here (a weak undefined symbol is an error for ld64). It is resolved at run
// time instead, and the pattern fallback is simply skipped when it is absent.
typedef void (*MSHookFunctionFn)(void* symbol, void* replacement, void** result);

MSHookFunctionFn MSHookFunctionPtr() {
  static MSHookFunctionFn fn =
      reinterpret_cast<MSHookFunctionFn>(dlsym(RTLD_DEFAULT, "MSHookFunction"));
  return fn;
}

// Outcome of one attempt on one table entry.
enum class Attempt {
  Done,    // installed, or deliberately skipped for good
  Retry,   // the runtime is not ready yet, try again later
};

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

// -1 = the class is not there yet (retry), 0 = not applicable, 1 = written.
int WriteStaticInt(Il2CppResolver& resolver, const HookEntry& e) {
  // The runtime offset of a static field is only known when the table carries
  // it. Writing a guessed offset would corrupt whatever sits at that slot,
  // so an unknown offset is skipped instead.
  if (e.field_offset == 0) return 0;
  ResolvedField f = resolver.resolveStaticField(e.type_name, e.method_name,
                                                e.field_offset);
  if (!f.valid()) return -1;
  *static_cast<int64_t*>(f.static_address) = e.int_value;
  return 1;
}

int WriteStaticString(Il2CppResolver& resolver, const HookEntry& e) {
  if (e.field_offset == 0) return 0;
  ResolvedField f = resolver.resolveStaticField(e.type_name, e.method_name,
                                                e.field_offset);
  if (!f.valid()) return -1;
  void* managed = resolver.newManagedString(e.string_value);
  if (!managed) return -1;
  *reinterpret_cast<void**>(f.static_address) = managed;
  return 1;
}

// One attempt on one entry. Report counters are only touched on success.
Attempt TryInstall(const HookEntry& e, Il2CppResolver& resolver,
                   InstallReport& report) {
  if (e.action == HookAction::SetStaticInt ||
      e.action == HookAction::SetStaticString) {
    int rc = (e.action == HookAction::SetStaticInt)
                 ? WriteStaticInt(resolver, e)
                 : WriteStaticString(resolver, e);
    if (rc == 1) {
      report.installed++;
      NSLog(@"[TIZAntiCheat] static %s.%s set to %lld", e.type_name.c_str(),
            e.method_name.c_str(), static_cast<long long>(e.int_value));
      return Attempt::Done;
    }
    if (rc == 0) {
      report.skipped++;
      NSLog(@"[TIZAntiCheat] static %s.%s not applied (no runtime offset yet)",
            e.type_name.c_str(), e.method_name.c_str());
      return Attempt::Done;
    }
    return Attempt::Retry;
  }

  void* detour = DetourFor(e.action);
  if (!detour) {
    report.skipped++;
    return Attempt::Done;
  }

  ResolvedMethod m = resolver.resolveMethod(e.type_name, e.method_name,
                                            ArgCountFor(e));
  if (!m.valid()) return Attempt::Retry;

  if (m.method_info) {
    // rewrite the method pointer in place
    if (resolver.setMethodPointer(m.method_info, detour)) {
      report.installed++;
      NSLog(@"[TIZAntiCheat] hooked %s.%s (method info %p)", e.type_name.c_str(),
            e.method_name.c_str(), m.method_info);
      return Attempt::Done;
    }
    report.failed++;
    NSLog(@"[TIZAntiCheat] method pointer write failed for %s.%s",
          e.type_name.c_str(), e.method_name.c_str());
    return Attempt::Done;
  }

  MSHookFunctionFn hook_fn = MSHookFunctionPtr();
  if (hook_fn) {
    void* original = nullptr;
    hook_fn(m.address, detour, &original);
    report.installed++;
    NSLog(@"[TIZAntiCheat] hooked %s.%s (pattern, %p)", e.type_name.c_str(),
          e.method_name.c_str(), m.address);
    return Attempt::Done;
  }

  report.failed++;
  NSLog(@"[TIZAntiCheat] no way to patch %s.%s (%p)", e.type_name.c_str(),
        e.method_name.c_str(), m.address);
  return Attempt::Done;
}

}  // namespace

// Installs every entry of the table. Returns a short report for the log.
//
// The IL2CPP runtime finishes loading its metadata after the game image
// appears, so an entry that cannot be resolved yet is retried instead of
// being written off. Without that, the hooks would be installed while the
// class table is still empty and the whole table would be a no-op.
InstallReport installHooks(const HookTable& table, Il2CppResolver& resolver) {
  InstallReport report;
  std::vector<const HookEntry*> pending;
  pending.reserve(table.entries().size());

  for (const HookEntry& e : table.entries()) {
    if (TryInstall(e, resolver, report) == Attempt::Retry) pending.push_back(&e);
  }

  const int kRounds = 120;          // 120 * 250 ms = 30 s of patience
  const useconds_t kPause = 250 * 1000;
  for (int round = 0; round < kRounds && !pending.empty(); ++round) {
    usleep(kPause);
    std::vector<const HookEntry*> still_pending;
    still_pending.reserve(pending.size());
    for (const HookEntry* e : pending) {
      if (TryInstall(*e, resolver, report) == Attempt::Retry) {
        still_pending.push_back(e);
      }
    }
    pending.swap(still_pending);
  }

  for (const HookEntry* e : pending) {
    report.failed++;
    NSLog(@"[TIZAntiCheat] unresolved %s.%s", e->type_name.c_str(),
          e->method_name.c_str());
  }
  return report;
}

}  // namespace tiz
