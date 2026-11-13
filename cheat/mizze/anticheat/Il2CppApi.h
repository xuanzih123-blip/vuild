// Il2CppApi.h - thin binding over the exported il2cpp_* runtime.
//
// Everything the anti ban layer needs is reached through the managed runtime
// by name, so a game update that renumbers tokens does not break it.
#pragma once

#include <cstddef>
#include <string>

namespace mizze {

class Il2CppApi {
 public:
  // Binds the API from the game image. Returns false when the image does not
  // export the runtime (then the managed layer cannot work at all).
  bool bind(void* image_base);

  bool ready() const { return class_from_name_ != nullptr && class_get_method_from_name_ != nullptr; }

  // True once the runtime finished initialising. The image shows up in the
  // dyld list before il2cpp_init() ran, and asking for a class before that
  // returns nothing.
  bool runtimeReady() const;

  // Managed class lookup. `ns` may be empty for the global namespace.
  void* classFromName(const char* ns, const char* cls) const;

  // Managed method lookup inside a class.
  void* methodFromName(void* klass, const char* name, int argc) const;

  // The native implementation behind a method.
  void* methodPointer(void* method_info) const;

  // Points a managed method at our own code. The game then calls us without
  // touching the original function, so the code pages stay byte identical and
  // a memory checksum over them still matches.
  bool setMethodPointer(void* method_info, void* detour) const;
  bool methodPointerMatches(void* method_info, void* detour) const;

 private:
  using FnClassFromName = void* (*)(const char*, const char*, const char*);
  using FnClassGetMethodFromName = void* (*)(void*, const char*, int);
  using FnDomainGet = void* (*)();
  using FnThreadCurrent = void* (*)();

  FnClassFromName class_from_name_ = nullptr;
  FnClassGetMethodFromName class_get_method_from_name_ = nullptr;
  FnDomainGet domain_get_ = nullptr;
  FnThreadCurrent thread_current_ = nullptr;
};

// Finds the game image among the loaded images.
void* findGameImage(size_t* size_out, std::string* name_out);

}  // namespace mizze
