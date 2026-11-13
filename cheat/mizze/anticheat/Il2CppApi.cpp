// Il2CppApi.cpp
#include "Il2CppApi.h"

#include <dlfcn.h>
#include <mach-o/dyld.h>
#include <mach-o/loader.h>
#include <cstring>

namespace mizze {
namespace {

// The IL2CPP layout of the method info structure. These are the offsets the
// runtime uses for every supported Unity generation.
constexpr uint32_t kMethodPointerOffset = 0x0;

size_t imageMappedSize(const struct mach_header* header) {
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
      uintptr_t start = static_cast<uintptr_t>(seg->vmaddr) -
                        reinterpret_cast<uintptr_t>(header);
      uintptr_t end = start + seg->vmsize;
      if (end > highest) highest = end;
    }
    cmd += lc->cmdsize;
  }
  return highest;
}

}  // namespace

void* findGameImage(size_t* size_out, std::string* name_out) {
  const uint32_t count = _dyld_image_count();
  for (uint32_t i = 0; i < count; ++i) {
    const char* raw = _dyld_get_image_name(i);
    if (!raw) continue;
    std::string name(raw);
    if (name.find("UnityFramework") == std::string::npos) continue;
    const struct mach_header* header = _dyld_get_image_header(i);
    if (!header) continue;
    size_t size = imageMappedSize(header);
    if (size == 0) continue;
    if (size_out) *size_out = size;
    if (name_out) *name_out = name;
    return const_cast<void*>(static_cast<const void*>(header));
  }
  return nullptr;
}

bool Il2CppApi::bind(void* image_base) {
  if (!image_base) return false;
  class_from_name_ = reinterpret_cast<FnClassFromName>(
      dlsym(image_base, "il2cpp_class_from_name"));
  class_get_method_from_name_ = reinterpret_cast<FnClassGetMethodFromName>(
      dlsym(image_base, "il2cpp_class_get_method_from_name"));
  domain_get_ = reinterpret_cast<FnDomainGet>(dlsym(image_base, "il2cpp_domain_get"));
  thread_current_ =
      reinterpret_cast<FnThreadCurrent>(dlsym(image_base, "il2cpp_thread_current"));
  return ready();
}

bool Il2CppApi::runtimeReady() const {
  if (domain_get_) return domain_get_() != nullptr;
  if (thread_current_) return thread_current_() != nullptr;
  return true;
}

void* Il2CppApi::classFromName(const char* ns, const char* cls) const {
  if (!class_from_name_) return nullptr;
  void* klass = class_from_name_("Assembly-CSharp", ns, cls);
  if (!klass) klass = class_from_name_(nullptr, ns, cls);
  return klass;
}

void* Il2CppApi::methodFromName(void* klass, const char* name, int argc) const {
  if (!class_get_method_from_name_ || !klass) return nullptr;
  return class_get_method_from_name_(klass, name, argc);
}

void* Il2CppApi::methodPointer(void* method_info) const {
  if (!method_info) return nullptr;
  void* pointer = nullptr;
  std::memcpy(&pointer,
              static_cast<const uint8_t*>(method_info) + kMethodPointerOffset,
              sizeof(pointer));
  return pointer;
}

bool Il2CppApi::setMethodPointer(void* method_info, void* detour) const {
  if (!method_info || !detour) return false;
  void** slot = reinterpret_cast<void**>(
      static_cast<uint8_t*>(method_info) + kMethodPointerOffset);
  *slot = detour;
  return true;
}

bool Il2CppApi::methodPointerMatches(void* method_info, void* detour) const {
  if (!method_info || !detour) return false;
  void** slot = reinterpret_cast<void**>(
      const_cast<uint8_t*>(static_cast<const uint8_t*>(method_info)) +
      kMethodPointerOffset);
  return *slot == detour;
}

}  // namespace mizze
