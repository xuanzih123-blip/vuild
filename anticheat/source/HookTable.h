// HookTable.h - minimal JSON reader plus the hook table.
//
// No third party dependency: the dylib has to stay self contained, so a tiny
// recursive descent JSON parser lives here. Only the subset the generator
// emits is supported (objects, arrays, strings, numbers, booleans, null).
#ifndef TIZ_HOOKTABLE_H
#define TIZ_HOOKTABLE_H

#include <map>
#include <string>
#include <vector>

#include "TIZAntiCheat.h"

namespace tiz {

class JsonValue {
 public:
  enum class Type { kNull, kBool, kNumber, kString, kArray, kObject };

  Type type = Type::kNull;
  bool boolean = false;
  double number = 0.0;
  std::string string;
  std::vector<JsonValue> array;
  std::map<std::string, JsonValue> object;

  bool isObject() const { return type == Type::kObject; }
  bool isArray() const { return type == Type::kArray; }
  bool isString() const { return type == Type::kString; }
  bool isNumber() const { return type == Type::kNumber; }

  const JsonValue* find(const std::string& key) const;
  std::string str(const std::string& key, const std::string& fallback = "") const;
  double num(const std::string& key, double fallback = 0.0) const;
  bool booleanValue(const std::string& key, bool fallback = false) const;
};

// Parses `text`. On failure returns false and fills `error`.
bool parseJson(const std::string& text, JsonValue* out, std::string* error);

class HookTable {
 public:
  // Loads the generated hook document. `error` receives a human readable
  // reason when the document is rejected.
  bool loadFromString(const std::string& json, std::string* error);
  bool loadFromFile(const std::string& path, std::string* error);

  const std::vector<HookEntry>& entries() const { return entries_; }
  const Il2CppProfile& profile() const { return profile_; }

 private:
  bool parseHooks(const JsonValue& root, std::string* error);
  bool parseProfile(const JsonValue& root);

  std::vector<HookEntry> entries_;
  Il2CppProfile profile_;
};

}  // namespace tiz

#endif  // TIZ_HOOKTABLE_H
