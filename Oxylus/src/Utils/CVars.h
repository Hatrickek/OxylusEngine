#pragma once

#include <shared_mutex>

#include <ankerl/unordered_dense.h>

#include <Utils/StringUtils.h>

#include "Log.h"

namespace Ox {
enum class CVarFlags : uint32_t {
  None         = 0,
  Noedit       = 1 << 1,
  EditReadOnly = 1 << 2,
  Advanced     = 1 << 3,
  Dropdown     = 1 << 4,

  EditCheckbox  = 1 << 8,
  EditFloatDrag = 1 << 9,
  EditIntDrag   = 1 << 10,
};

enum class CVarType : char {
  INT,
  FLOAT,
  STRING,
};

class CVarParameter {
public:
  uint32_t array_index;

  CVarType type;
  CVarFlags flags;
  std::string name;
  std::string description;
};

template <typename T>
struct CVarStorage {
  T initial;
  T current;
  CVarParameter* parameter;
};

class CVarSystem {
public:
  constexpr static int MAX_INT_CVARS = 1000;
  constexpr static int MAX_FLOAT_CVARS = 1000;
  constexpr static int MAX_STRING_CVARS = 200;

  std::vector<CVarStorage<int32_t>> int_cvars = {};
  std::vector<CVarStorage<float>> float_cvars = {};
  std::vector<CVarStorage<std::string>> string_cvars = {};

  CVarSystem() = default;

  static CVarSystem* get();
  CVarParameter* get_cvar(StringUtils::StringHash hash);

  CVarParameter* create_float_cvar(const char* name, const char* description, float default_value, float current_value);
  CVarParameter* create_int_cvar(const char* name, const char* description, int32_t default_value, int32_t current_value);
  CVarParameter* create_string_cvar(const char* name, const char* description, const char* default_value, const char* current_value);

  float* get_float_cvar(StringUtils::StringHash hash);
  int32_t* get_int_cvar(StringUtils::StringHash hash);
  const char* get_string_cvar(StringUtils::StringHash hash);

  void set_float_cvar(StringUtils::StringHash hash, float value);
  void set_int_cvar(StringUtils::StringHash hash, int32_t value);
  void set_string_cvar(StringUtils::StringHash hash, const char* value);

  template <typename T>
  std::vector<CVarStorage<T>>* get_cvar_array();

  template <>
  std::vector<CVarStorage<int32_t>>* get_cvar_array() {
    return &int_cvars;
  }

  template <>
  std::vector<CVarStorage<float>>* get_cvar_array() {
    return &float_cvars;
  }

  template <>
  std::vector<CVarStorage<std::string>>* get_cvar_array() {
    return &string_cvars;
  }

  //templated get-set cvar versions for syntax sugar
  template <typename T>
  T* get_cvar_current(const uint32_t namehash) {
    CVarParameter* par = get_cvar(namehash);
    if (!par) {
      OX_CORE_ERROR("cvar {} doesn't exists!", namehash);
      return nullptr;
    }
    return &get_cvar_array<T>()->at(par->array_index).current;
  }

  template <typename T>
  void set_cvar_current(const uint32_t namehash, const T& value) {
    CVarParameter* cvar = get_cvar(namehash);
    if (cvar) {
      get_cvar_array<T>()->at(cvar->array_index).current = value;
    }
  }

private:
  std::shared_mutex mutex_;
  std::unordered_map<uint32_t, CVarParameter> saved_cvars;
  std::vector<CVarParameter*> cached_edit_parameters;

  CVarParameter* init_cvar(const char* name, const char* description);
};

template <typename T>
struct AutoCVar {
protected:
  uint32_t index;
  using CVarType = T;
};

struct AutoCVar_Float : AutoCVar<float> {
  AutoCVar_Float(const char* name, const char* description, float default_value, CVarFlags flags = CVarFlags::None);

  float get() const;
  float* get_ptr() const;
  float get_float() const;
  float* get_float_ptr() const;
  void set(float val) const;
};

struct AutoCVar_Int : AutoCVar<int32_t> {
  AutoCVar_Int(const char* name, const char* description, int32_t default_value, CVarFlags flags = CVarFlags::None);
  int32_t get() const;
  int32_t* get_ptr() const;
  void set(int32_t val) const;

  void toggle() const;
};

struct AutoCVar_String : AutoCVar<std::string> {
  AutoCVar_String(const char* name, const char* description, const char* default_value, CVarFlags flags = CVarFlags::None);

  std::string get() const;
  void set(std::string&& val) const;
};
}
