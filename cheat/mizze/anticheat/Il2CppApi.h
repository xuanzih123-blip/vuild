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

  // Managed field lookup inside a class.
  void* fieldFromName(void* klass, const char* name) const;

  // The offset of a field inside its object, computed by the runtime. This is
  // the piece that lets a static field be written without a hard coded
  // profile: the offset is whatever the running game uses.
  int fieldOffset(void* field) const;

  // Writes a static field. The value is read back from the given pointer.
  bool setStaticValue(void* field, const void* value) const;

  // Creates a managed string, needed for the static string fields.
  void* newManagedString(const char* utf8) const;

  // --- profile probe ----------------------------------------------------
  // Walks the fields of a class and of every base class. The visitor gets
  // the declaring class, the field name and the offset of the field inside
  // the object. This is how the offset profile of the game is verified on
  // the device instead of being guessed.
  typedef void (*FieldVisitor)(const char* declaring_class, const char* field_name,
                               int offset, void* context);
  void forEachField(void* klass, FieldVisitor visitor, void* context) const;

  // The name of a class, "COW.GamePlay.Player".
  std::string className(void* klass) const;

  // The size of an instance of the class in bytes.
  int classInstanceSize(void* klass) const;

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
  using FnClassGetFieldFromName = void* (*)(void*, const char*);
  using FnFieldGetOffset = int (*)(void*);
  using FnFieldStaticSetValue = void (*)(void*, void*);
  using FnStringNew = void* (*)(const char*);
  using FnClassGetFields = void* (*)(void*, void**);
  using FnFieldGetName = const char* (*)(void*);
  using FnFieldGetParent = void* (*)(void*);
  using FnClassGetName = const char* (*)(void*);
  using FnClassGetNamespace = const char* (*)(void*);
  using FnClassGetParent = void* (*)(void*);
  using FnClassInstanceSize = int (*)(void*);
  using FnDomainGet = void* (*)();
  using FnThreadCurrent = void* (*)();

  FnClassFromName class_from_name_ = nullptr;
  FnClassGetMethodFromName class_get_method_from_name_ = nullptr;
  FnClassGetFieldFromName class_get_field_from_name_ = nullptr;
  FnFieldGetOffset field_get_offset_ = nullptr;
  FnFieldStaticSetValue field_static_set_value_ = nullptr;
  FnStringNew string_new_ = nullptr;
  FnClassGetFields class_get_fields_ = nullptr;
  FnFieldGetName field_get_name_ = nullptr;
  FnFieldGetParent field_get_parent_ = nullptr;
  FnClassGetName class_get_name_ = nullptr;
  FnClassGetNamespace class_get_namespace_ = nullptr;
  FnClassGetParent class_get_parent_ = nullptr;
  FnClassInstanceSize class_instance_size_ = nullptr;
  FnDomainGet domain_get_ = nullptr;
  FnThreadCurrent thread_current_ = nullptr;
};

// Finds the game image among the loaded images.
void* findGameImage(size_t* size_out, std::string* name_out);

}  // namespace mizze
