// Il2CppResolver.cpp
#include "Il2CppResolver.h"

#include <cstring>

#if defined(__APPLE__)
#include <dlfcn.h>
#include <mach-o/loader.h>
#endif

namespace tiz {
namespace {

uint32_t readU32(const void* base, uint32_t offset) {
  uint32_t v = 0;
  std::memcpy(&v, reinterpret_cast<const uint8_t*>(base) + offset, sizeof(v));
  return v;
}

uint64_t readU64(const void* base, uint32_t offset) {
  uint64_t v = 0;
  std::memcpy(&v, reinterpret_cast<const uint8_t*>(base) + offset, sizeof(v));
  return v;
}

const char* readCStr(const void* base, uint32_t offset) {
  return reinterpret_cast<const char*>(reinterpret_cast<const uint8_t*>(base) + offset);
}

}  // namespace

void Il2CppResolver::setGameImage(void* base, size_t size, const std::string& name) {
  game_base_ = base;
  game_size_ = size;
  game_image_name_ = name;
}

bool Il2CppResolver::bindExportedApi(void* image_base) {
#if defined(__APPLE__)
  if (!image_base) return false;
  class_from_name_ = reinterpret_cast<FnClassFromName>(
      dlsym(image_base, "il2cpp_class_from_name"));
  class_get_method_from_name_ = reinterpret_cast<FnClassGetMethodFromName>(
      dlsym(image_base, "il2cpp_class_get_method_from_name"));
  class_get_field_from_name_ = reinterpret_cast<FnClassGetFieldFromName>(
      dlsym(image_base, "il2cpp_class_get_field_from_name"));
  domain_get_ = reinterpret_cast<FnDomainGet>(dlsym(image_base, "il2cpp_domain_get"));
  thread_current_ =
      reinterpret_cast<FnThreadCurrent>(dlsym(image_base, "il2cpp_thread_current"));
  return class_from_name_ != nullptr && class_get_method_from_name_ != nullptr;
#else
  (void)image_base;
  return false;
#endif
}

bool Il2CppResolver::runtimeReady() const {
  // The game image is in the dyld list before il2cpp_init() ran. Asking the
  // runtime for a class before that returns nothing, so the caller waits.
  if (domain_get_) return domain_get_() != nullptr;
  if (thread_current_) return thread_current_() != nullptr;
  return true;  // no probe exported: the caller has to try and see
}

bool Il2CppResolver::bindStringApi(void* image_base) {
#if defined(__APPLE__)
  if (!image_base) return false;
  string_new_ = reinterpret_cast<FnStringNew>(dlsym(image_base, "il2cpp_string_new"));
  return string_new_ != nullptr;
#else
  (void)image_base;
  return false;
#endif
}

void* Il2CppResolver::newManagedString(const std::string& utf8) const {
  if (!string_new_) return nullptr;
  return string_new_(utf8.c_str());
}

void Il2CppResolver::addPattern(const std::string& type_name,
                                 const std::string& method_name,
                                 const std::string& pattern_hex) {
  patterns_.push_back({type_name, method_name, pattern_hex});
}

void* Il2CppResolver::scanPattern(const std::string& pattern_hex) const {
  if (!game_base_ || pattern_hex.empty()) return nullptr;
  // "AA BB ?? CC" -> bytes with 0x00 as the wildcard
  std::vector<uint8_t> bytes;
  std::vector<bool> mask;
  size_t i = 0;
  while (i < pattern_hex.size()) {
    while (i < pattern_hex.size() && pattern_hex[i] == ' ') ++i;
    if (i + 1 >= pattern_hex.size()) break;
    std::string tok = pattern_hex.substr(i, 2);
    i += 2;
    while (i < pattern_hex.size() && pattern_hex[i] == ' ') ++i;
    if (tok == "??") {
      bytes.push_back(0);
      mask.push_back(false);
    } else {
      bytes.push_back(static_cast<uint8_t>(std::strtoul(tok.c_str(), nullptr, 16)));
      mask.push_back(true);
    }
  }
  if (bytes.empty()) return nullptr;
  const uint8_t* base = reinterpret_cast<const uint8_t*>(game_base_);
  size_t limit = game_size_ > bytes.size() ? game_size_ - bytes.size() : 0;
  for (size_t off = 0; off < limit; ++off) {
    size_t k = 0;
    for (; k < bytes.size(); ++k) {
      if (mask[k] && base[off + k] != bytes[k]) break;
    }
    if (k == bytes.size()) return const_cast<uint8_t*>(base) + off;
  }
  return nullptr;
}

ResolvedMethod Il2CppResolver::resolveMethod(const std::string& type_name,
                                             const std::string& method_name,
                                             int arg_count) {
  ResolvedMethod out;
  out.arg_count = arg_count;

  // strategy 1: exported il2cpp API
  if (class_from_name_ && class_get_method_from_name_) {
    std::string ns;
    std::string cls = type_name;
    size_t dot = type_name.find('.');
    if (dot != std::string::npos) {
      ns = type_name.substr(0, dot);
      cls = type_name.substr(dot + 1);
    }
    void* klass = class_from_name_("Assembly-CSharp", ns.c_str(), cls.c_str());
    if (!klass) klass = class_from_name_(nullptr, ns.c_str(), cls.c_str());
    if (klass) {
      void* info = class_get_method_from_name_(klass, method_name.c_str(), arg_count);
      if (info) {
        out.method_info = info;
        out.address = reinterpret_cast<void*>(readU64(info, profile_.method_info_method_pointer));
        out.name = readCStr(info, profile_.method_info_name);
        if (out.address) return out;
      }
    }
  }

  // strategy 2: pattern table
  for (const auto& p : patterns_) {
    if (p.type_name != type_name || p.method_name != method_name) continue;
    void* addr = scanPattern(p.pattern_hex);
    if (addr) {
      out.address = addr;
      out.name = method_name.c_str();
      return out;
    }
  }

  last_error_ = "could not resolve " + type_name + "::" + method_name;
  return out;
}

ResolvedField Il2CppResolver::resolveStaticField(const std::string& type_name,
                                                 const std::string& field_name,
                                                 uint32_t field_offset) {
  ResolvedField out;
  if (!class_from_name_ || !class_get_field_from_name_) return out;
  std::string ns;
  std::string cls = type_name;
  size_t dot = type_name.find('.');
  if (dot != std::string::npos) {
    ns = type_name.substr(0, dot);
    cls = type_name.substr(dot + 1);
  }
  void* klass = class_from_name_("Assembly-CSharp", ns.c_str(), cls.c_str());
  if (!klass) klass = class_from_name_(nullptr, ns.c_str(), cls.c_str());
  if (!klass) return out;
  void* field = class_get_field_from_name_(klass, field_name.c_str());
  if (!field) return out;
  void* static_fields = reinterpret_cast<void*>(
      readU64(klass, profile_.class_static_fields));
  if (!static_fields) return out;
  uint32_t real_offset = field_offset;
  if (real_offset == 0) {
    real_offset = readU32(field, profile_.field_info_offset);
  }
  out.static_address = static_cast<uint8_t*>(static_fields) + real_offset;
  return out;
}

bool Il2CppResolver::setMethodPointer(void* method_info, void* detour) {
  if (!method_info || !detour) return false;
  void** slot = reinterpret_cast<void**>(
      static_cast<uint8_t*>(method_info) + profile_.method_info_method_pointer);
  *slot = detour;
  return true;
}

}  // namespace tiz
