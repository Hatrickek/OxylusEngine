#pragma once

#include <Utils/StringUtils.h>

namespace Oxylus {
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
  friend class CVarSystemImpl;

  int32_t arrayIndex;

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

template <typename T>
struct CVarArray {
  CVarStorage<T>* cvars{nullptr};
  int32_t lastCVar{0};

  CVarArray(size_t size) {
    cvars = new CVarStorage<T>[size]();
  }


  CVarStorage<T>* get_current_storage(int32_t index) {
    return &cvars[index];
  }

  T* get_current_ptr(int32_t index) {
    return &cvars[index].current;
  }

  T get_current(int32_t index) {
    return cvars[index].current;
  }

  void set_current(const T& val, int32_t index) {
    cvars[index].current = val;
  }

  int add(const T& value, CVarParameter* param) {
    int index = lastCVar;

    cvars[index].current = value;
    cvars[index].initial = value;
    cvars[index].parameter = param;

    param->arrayIndex = index;
    lastCVar++;
    return index;
  }

  int add(const T& initialValue, const T& currentValue, CVarParameter* param) {
    int index = lastCVar;

    cvars[index].current = currentValue;
    cvars[index].initial = initialValue;
    cvars[index].parameter = param;

    param->arrayIndex = index;
    lastCVar++;

    return index;
  }
};

class CVarSystem {
public:
  virtual ~CVarSystem() = default;
  static CVarSystem* get();

  virtual CVarParameter* get_cvar(StringUtils::StringHash hash) = 0;

  virtual float* get_float_cvar(StringUtils::StringHash hash) = 0;
  virtual int32_t* get_int_cvar(StringUtils::StringHash hash) = 0;
  virtual const char* get_string_cvar(StringUtils::StringHash hash) = 0;

  virtual void set_float_cvar(StringUtils::StringHash hash, float value) = 0;
  virtual void set_int_cvar(StringUtils::StringHash hash, int32_t value) = 0;
  virtual void set_string_cvar(StringUtils::StringHash hash, const char* value) = 0;

  virtual CVarParameter* create_float_cvar(const char* name, const char* description, float defaultValue, float currentValue) = 0;
  virtual CVarParameter* create_int_cvar(const char* name, const char* description, int32_t defaultValue, int32_t currentValue) = 0;
  virtual CVarParameter* create_string_cvar(const char* name, const char* description, const char* defaultValue, const char* currentValue) = 0;

  constexpr static int MAX_INT_CVARS = 1000;
  CVarArray<int32_t> int_cvars2{MAX_INT_CVARS};

  constexpr static int MAX_FLOAT_CVARS = 1000;
  CVarArray<float> float_cvars{MAX_FLOAT_CVARS};

  constexpr static int MAX_STRING_CVARS = 200;
  CVarArray<std::string> string_cvars{MAX_STRING_CVARS};

  template <typename T>
  CVarArray<T>* get_cvar_array();

  template <>
  CVarArray<int32_t>* get_cvar_array() {
    return &int_cvars2;
  }

  template <>
  CVarArray<float>* get_cvar_array() {
    return &float_cvars;
  }

  template <>
  CVarArray<std::string>* get_cvar_array() {
    return &string_cvars;
  }
};

template <typename T>
struct AutoCVar {
protected:
  int index;
  using CVarType = T;
};

struct AutoCVar_Float : AutoCVar<float> {
  AutoCVar_Float(const char* name, const char* description, float defaultValue, CVarFlags flags = CVarFlags::None);

  float get() const;
  float* get_ptr() const;
  float get_float() const;
  float* get_float_ptr() const;
  void set(float val) const;
};

struct AutoCVar_Int : AutoCVar<int32_t> {
  AutoCVar_Int(const char* name, const char* description, int32_t defaultValue, CVarFlags flags = CVarFlags::None);
  int32_t get() const;
  int32_t* get_ptr() const;
  void set(int32_t val) const;

  void toggle() const;
};

struct AutoCVar_String : AutoCVar<std::string> {
  AutoCVar_String(const char* name, const char* description, const char* defaultValue, CVarFlags flags = CVarFlags::None);

  std::string get() const;
  void set(std::string&& val) const;
};
}
