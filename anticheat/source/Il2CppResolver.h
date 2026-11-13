// Il2CppResolver.h - turn a managed class/method name into a native pointer.
//
// Three strategies, tried in order:
//   1. the il2cpp_* API exported by the game image
//   2. a byte pattern table (produced by tools/gen_patterns.py from the game
//      binary) scanned inside the game image
//   3. the Il2CppCodeRegistration method pointer table, when the registration
//      structure can be located
#ifndef TIZ_IL2CPPRESOLVER_H
#define TIZ_IL2CPPRESOLVER_H

#include <cstdint>
#include <string>
#include <vector>

#include "TIZAntiCheat.h"

namespace tiz {

struct ResolvedMethod {
  void* address = nullptr;    // native implementation
  void* method_info = nullptr;  // Il2CppMethodInfo*, when known
  const char* name = nullptr;
  int arg_count = 0;
  bool valid() const { return address != nullptr; }
};

struct ResolvedField {
  void* static_address = nullptr;  // address of the static storage
  bool valid() const { return static_address != nullptr; }
};

class Il2CppResolver {
 public:
  explicit Il2CppResolver(const Il2CppProfile& profile) : profile_(profile) {}

  // Points the resolver at the game image (UnityFramework / libil2cpp.so).
  void setGameImage(void* base, size_t size, const std::string& name);

  // Strategy 1: exported il2cpp API.
  bool bindExportedApi(void* image_base);

  // True once the IL2CPP runtime is up. The game image shows up in the dyld
  // list before il2cpp_init() ran, and asking for a class before that returns
  // nothing (or worse), so the caller waits for this before installing.
  bool runtimeReady() const;

  // Optional extra API: needed to build managed strings for static string
  // fields. Returns false when the game does not export it.
  bool bindStringApi(void* image_base);

  // Strategy 2: pattern table.
  void addPattern(const std::string& type_name, const std::string& method_name,
                  const std::string& pattern_hex);

  // Looks a managed method up.
  ResolvedMethod resolveMethod(const std::string& type_name,
                                const std::string& method_name, int arg_count);

  // Looks a static field up (returns the address of its storage).
  ResolvedField resolveStaticField(const std::string& type_name,
                                   const std::string& field_name,
                                   uint32_t field_offset);

  // Overwrites Il2CppMethodInfo::methodPointer so every call to the managed
  // method lands in `detour`. This is the preferred hooking mechanism: it
  // needs no Substrate and it cannot corrupt the target function.
  bool setMethodPointer(void* method_info, void* detour);

  // True when the method pointer still points at `detour`. Used to detect a
  // hotfix that restored the original implementation.
  bool methodPointerMatches(void* method_info, void* detour) const;

  // Creates a managed string (Il2CppString*) for static string fields.
  void* newManagedString(const std::string& utf8) const;

  const std::string& gameImageName() const { return game_image_name_; }
  bool hasExportedApi() const { return class_from_name_ != nullptr; }
  std::string lastError() const { return last_error_; }

 private:
  void* scanPattern(const std::string& pattern_hex) const;

  Il2CppProfile profile_;
  void* game_base_ = nullptr;
  size_t game_size_ = 0;
  std::string game_image_name_;
  std::string last_error_;

  using FnClassFromName = void* (*)(const char*, const char*, const char*);
  using FnClassGetMethodFromName = void* (*)(void*, const char*, int);
  using FnClassGetFieldFromName = void* (*)(void*, const char*);
  // Readiness probes: both are exported by a stock IL2CPP build and both
  // return null until the runtime is initialised.
  using FnDomainGet = void* (*)();
  using FnThreadCurrent = void* (*)();

  FnDomainGet domain_get_ = nullptr;
  FnThreadCurrent thread_current_ = nullptr;

  FnClassFromName class_from_name_ = nullptr;
  FnClassGetMethodFromName class_get_method_from_name_ = nullptr;
  FnClassGetFieldFromName class_get_field_from_name_ = nullptr;
  using FnStringNew = void* (*)(const char*);
  FnStringNew string_new_ = nullptr;

  struct PatternEntry {
    std::string type_name;
    std::string method_name;
    std::string pattern_hex;
  };
  std::vector<PatternEntry> patterns_;
};

}  // namespace tiz

#endif  // TIZ_IL2CPPRESOLVER_H
