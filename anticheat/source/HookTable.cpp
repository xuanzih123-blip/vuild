// HookTable.cpp - JSON reader and hook table loader.
#include "HookTable.h"

#include <cctype>
#include <cstdlib>
#include <fstream>
#include <sstream>

namespace tiz {
namespace {

class Parser {
 public:
  Parser(const std::string& text) : s_(text) {}

  bool parse(JsonValue* out, std::string* error) {
    skipSpace();
    if (!parseValue(out, error)) return false;
    skipSpace();
    if (pos_ != s_.size()) {
      *error = "trailing data at offset " + std::to_string(pos_);
      return false;
    }
    return true;
  }

 private:
  void skipSpace() {
    while (pos_ < s_.size()) {
      unsigned char c = static_cast<unsigned char>(s_[pos_]);
      if (c == ' ' || c == '\t' || c == '\n' || c == '\r') {
        ++pos_;
      } else {
        break;
      }
    }
  }

  bool literal(const char* text) {
    size_t n = std::char_traits<char>::length(text);
    if (s_.compare(pos_, n, text) != 0) return false;
    pos_ += n;
    return true;
  }

  bool parseValue(JsonValue* out, std::string* error) {
    skipSpace();
    if (pos_ >= s_.size()) {
      *error = "unexpected end of input";
      return false;
    }
    char c = s_[pos_];
    if (c == '{') return parseObject(out, error);
    if (c == '[') return parseArray(out, error);
    if (c == '"') {
      out->type = JsonValue::Type::kString;
      return parseString(&out->string, error);
    }
    if (literal("true")) {
      out->type = JsonValue::Type::kBool;
      out->boolean = true;
      return true;
    }
    if (literal("false")) {
      out->type = JsonValue::Type::kBool;
      out->boolean = false;
      return true;
    }
    if (literal("null")) {
      out->type = JsonValue::Type::kNull;
      return true;
    }
    // number
    size_t start = pos_;
    if (pos_ < s_.size() && (s_[pos_] == '-' || s_[pos_] == '+')) ++pos_;
    while (pos_ < s_.size() &&
           (std::isdigit(static_cast<unsigned char>(s_[pos_])) || s_[pos_] == '.' ||
            s_[pos_] == 'e' || s_[pos_] == 'E' || s_[pos_] == '-' || s_[pos_] == '+')) {
      ++pos_;
    }
    if (pos_ == start) {
      *error = std::string("unexpected character '") + c + "' at offset " +
               std::to_string(pos_);
      return false;
    }
    out->type = JsonValue::Type::kNumber;
    out->number = std::strtod(s_.substr(start, pos_ - start).c_str(), nullptr);
    return true;
  }

  bool parseString(std::string* out, std::string* error) {
    if (s_[pos_] != '"') {
      *error = "expected '\"' at offset " + std::to_string(pos_);
      return false;
    }
    ++pos_;
    out->clear();
    while (pos_ < s_.size()) {
      char c = s_[pos_++];
      if (c == '"') return true;
      if (c != '\\') {
        out->push_back(c);
        continue;
      }
      if (pos_ >= s_.size()) break;
      char e = s_[pos_++];
      switch (e) {
        case 'n': out->push_back('\n'); break;
        case 't': out->push_back('\t'); break;
        case 'r': out->push_back('\r'); break;
        case 'b': out->push_back('\b'); break;
        case 'f': out->push_back('\f'); break;
        case '/': out->push_back('/'); break;
        case '"': out->push_back('"'); break;
        case '\\': out->push_back('\\'); break;
        case 'u': {
          if (pos_ + 4 > s_.size()) {
            *error = "truncated \\u escape";
            return false;
          }
          unsigned code = 0;
          for (int i = 0; i < 4; ++i) {
            char h = s_[pos_++];
            code <<= 4;
            if (h >= '0' && h <= '9') code |= static_cast<unsigned>(h - '0');
            else if (h >= 'a' && h <= 'f') code |= static_cast<unsigned>(h - 'a' + 10);
            else if (h >= 'A' && h <= 'F') code |= static_cast<unsigned>(h - 'A' + 10);
          }
          // UTF-8 encode (BMP only, surrogate pairs are passed through)
          if (code < 0x80) {
            out->push_back(static_cast<char>(code));
          } else if (code < 0x800) {
            out->push_back(static_cast<char>(0xC0 | (code >> 6)));
            out->push_back(static_cast<char>(0x80 | (code & 0x3F)));
          } else {
            out->push_back(static_cast<char>(0xE0 | (code >> 12)));
            out->push_back(static_cast<char>(0x80 | ((code >> 6) & 0x3F)));
            out->push_back(static_cast<char>(0x80 | (code & 0x3F)));
          }
          break;
        }
        default:
          out->push_back(e);
          break;
      }
    }
    *error = "unterminated string";
    return false;
  }

  bool parseArray(JsonValue* out, std::string* error) {
    out->type = JsonValue::Type::kArray;
    ++pos_;  // '['
    skipSpace();
    if (pos_ < s_.size() && s_[pos_] == ']') {
      ++pos_;
      return true;
    }
    while (true) {
      JsonValue v;
      if (!parseValue(&v, error)) return false;
      out->array.push_back(v);
      skipSpace();
      if (pos_ >= s_.size()) {
        *error = "unterminated array";
        return false;
      }
      if (s_[pos_] == ',') {
        ++pos_;
        continue;
      }
      if (s_[pos_] == ']') {
        ++pos_;
        return true;
      }
      *error = "expected ',' or ']' at offset " + std::to_string(pos_);
      return false;
    }
  }

  bool parseObject(JsonValue* out, std::string* error) {
    out->type = JsonValue::Type::kObject;
    ++pos_;  // '{'
    skipSpace();
    if (pos_ < s_.size() && s_[pos_] == '}') {
      ++pos_;
      return true;
    }
    while (true) {
      skipSpace();
      std::string key;
      if (!parseString(&key, error)) return false;
      skipSpace();
      if (pos_ >= s_.size() || s_[pos_] != ':') {
        *error = "expected ':' at offset " + std::to_string(pos_);
        return false;
      }
      ++pos_;
      JsonValue v;
      if (!parseValue(&v, error)) return false;
      out->object[key] = v;
      skipSpace();
      if (pos_ >= s_.size()) {
        *error = "unterminated object";
        return false;
      }
      if (s_[pos_] == ',') {
        ++pos_;
        continue;
      }
      if (s_[pos_] == '}') {
        ++pos_;
        return true;
      }
      *error = "expected ',' or '}' at offset " + std::to_string(pos_);
      return false;
    }
  }

  const std::string& s_;
  size_t pos_ = 0;
};

}  // namespace

const JsonValue* JsonValue::find(const std::string& key) const {
  auto it = object.find(key);
  return it == object.end() ? nullptr : &it->second;
}

std::string JsonValue::str(const std::string& key, const std::string& fallback) const {
  const JsonValue* v = find(key);
  return (v && v->isString()) ? v->string : fallback;
}

double JsonValue::num(const std::string& key, double fallback) const {
  const JsonValue* v = find(key);
  return (v && v->isNumber()) ? v->number : fallback;
}

bool JsonValue::booleanValue(const std::string& key, bool fallback) const {
  const JsonValue* v = find(key);
  return (v && v->type == JsonValue::Type::kBool) ? v->boolean : fallback;
}

bool parseJson(const std::string& text, JsonValue* out, std::string* error) {
  Parser p(text);
  std::string local;
  if (!p.parse(out, &local)) {
    if (error) *error = local;
    return false;
  }
  return true;
}

const char* actionName(HookAction a) {
  switch (a) {
    case HookAction::ReturnFalse: return "return_false";
    case HookAction::ReturnTrue: return "return_true";
    case HookAction::Skip: return "skip";
    case HookAction::SetStaticInt: return "set_static_int";
    case HookAction::SetStaticString: return "set_static_string";
  }
  return "skip";
}

bool actionFromName(const std::string& name, HookAction* out) {
  if (name == "return_false") { *out = HookAction::ReturnFalse; return true; }
  if (name == "return_true") { *out = HookAction::ReturnTrue; return true; }
  if (name == "skip") { *out = HookAction::Skip; return true; }
  if (name == "set_static_int") { *out = HookAction::SetStaticInt; return true; }
  if (name == "set_static_string") { *out = HookAction::SetStaticString; return true; }
  return false;
}

bool HookTable::loadFromString(const std::string& json, std::string* error) {
  JsonValue root;
  if (!parseJson(json, &root, error)) return false;
  if (!root.isObject()) {
    if (error) *error = "root value is not an object";
    return false;
  }
  if (!parseProfile(root)) {
    if (error) *error = "invalid il2cpp profile";
    return false;
  }
  return parseHooks(root, error);
}

bool HookTable::loadFromFile(const std::string& path, std::string* error) {
  std::ifstream in(path, std::ios::binary);
  if (!in) {
    if (error) *error = "cannot open " + path;
    return false;
  }
  std::ostringstream ss;
  ss << in.rdbuf();
  return loadFromString(ss.str(), error);
}

bool HookTable::parseProfile(const JsonValue& root) {
  const JsonValue* p = root.find("il2cpp");
  if (!p || !p->isObject()) return true;  // defaults are fine
  const JsonValue* mi = p->find("method_info");
  if (mi && mi->isObject()) {
    profile_.method_info_method_pointer =
        static_cast<uint32_t>(mi->num("method_pointer_offset", profile_.method_info_method_pointer));
    profile_.method_info_name =
        static_cast<uint32_t>(mi->num("name_offset", profile_.method_info_name));
    profile_.method_info_klass =
        static_cast<uint32_t>(mi->num("klass_offset", profile_.method_info_klass));
    profile_.method_info_token =
        static_cast<uint32_t>(mi->num("token_offset", profile_.method_info_token));
  }
  const JsonValue* ci = p->find("class_info");
  if (ci && ci->isObject()) {
    profile_.class_static_fields =
        static_cast<uint32_t>(ci->num("static_fields_offset", profile_.class_static_fields));
    profile_.class_name =
        static_cast<uint32_t>(ci->num("name_offset", profile_.class_name));
    profile_.class_namespace =
        static_cast<uint32_t>(ci->num("namespace_offset", profile_.class_namespace));
  }
  const JsonValue* fi = p->find("field_info");
  if (fi && fi->isObject()) {
    profile_.field_info_offset =
        static_cast<uint32_t>(fi->num("offset_offset", profile_.field_info_offset));
    profile_.field_info_name =
        static_cast<uint32_t>(fi->num("name_offset", profile_.field_info_name));
  }
  return true;
}

bool HookTable::parseHooks(const JsonValue& root, std::string* error) {
  const JsonValue* hooks = root.find("hooks");
  if (!hooks || !hooks->isArray()) {
    if (error) *error = "missing \"hooks\" array";
    return false;
  }
  for (size_t i = 0; i < hooks->array.size(); ++i) {
    const JsonValue& h = hooks->array[i];
    if (!h.isObject()) {
      if (error) *error = "hook #" + std::to_string(i) + " is not an object";
      return false;
    }
    HookEntry e;
    e.type_name = h.str("type");
    e.method_name = h.str("method");
    e.note = h.str("note");
    e.token = static_cast<uint32_t>(h.num("token", 0));
    e.arg_count = static_cast<int>(h.num("arg_count", 0));
    e.field_offset = static_cast<uint32_t>(h.num("field_offset", 0));
    std::string action = h.str("action", "skip");
    if (!actionFromName(action, &e.action)) {
      if (error) *error = "unknown action '" + action + "' in hook #" + std::to_string(i);
      return false;
    }
    e.int_value = static_cast<int64_t>(h.num("int_value", 0));
    e.string_value = h.str("string_value");
    if (e.type_name.empty() || e.method_name.empty()) {
      if (error) *error = "hook #" + std::to_string(i) + " lacks type or method";
      return false;
    }
    entries_.push_back(e);
  }
  return true;
}

}  // namespace tiz
