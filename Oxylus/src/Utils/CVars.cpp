#include "Utils/CVars.h"

#include <unordered_map>

#include <shared_mutex>

#include "Log.h"
#include "Profiler.h"

namespace Oxylus {
class CVarSystemImpl final : public CVarSystem {
public:
  CVarParameter* get_cvar(StringUtils::StringHash hash) override;

  CVarParameter* create_float_cvar(const char* name, const char* description, float defaultValue, float currentValue) override;
  CVarParameter* create_int_cvar(const char* name, const char* description, int32_t defaultValue, int32_t currentValue) override;
  CVarParameter* create_string_cvar(const char* name, const char* description, const char* defaultValue, const char* currentValue) override;

  float* get_float_cvar(StringUtils::StringHash hash) override;
  int32_t* get_int_cvar(StringUtils::StringHash hash) override;
  const char* get_string_cvar(StringUtils::StringHash hash) override;

  void set_float_cvar(StringUtils::StringHash hash, float value) override;
  void set_int_cvar(StringUtils::StringHash hash, int32_t value) override;
  void set_string_cvar(StringUtils::StringHash hash, const char* value) override;

  //templated get-set cvar versions for syntax sugar
  template <typename T>
  T* get_cvar_current(const uint32_t namehash) {
    CVarParameter* par = get_cvar(namehash);
    if (!par) {
      OX_CORE_ERROR("cvar {} doesn't exists!",namehash);
      return nullptr;
    }
    return get_cvar_array<T>()->get_current_ptr(par->arrayIndex);
  }

  template <typename T>
  void set_cvar_current(const uint32_t namehash, const T& value) {
    CVarParameter* cvar = get_cvar(namehash);
    if (cvar) {
      get_cvar_array<T>()->set_current(value, cvar->arrayIndex);
    }
  }

  static CVarSystemImpl* get() {
    return dynamic_cast<CVarSystemImpl*>(CVarSystem::get());
  }

private:
  std::shared_mutex mutex_;
  std::unordered_map<uint32_t, CVarParameter> saved_cvars;
  std::vector<CVarParameter*> cached_edit_parameters;

  CVarParameter* init_cvar(const char* name, const char* description);
};

float* CVarSystemImpl::get_float_cvar(const StringUtils::StringHash hash) {
  OX_SCOPED_ZONE;
  return get_cvar_current<float>(hash);
}

int32_t* CVarSystemImpl::get_int_cvar(const StringUtils::StringHash hash) {
  OX_SCOPED_ZONE;
  return get_cvar_current<int32_t>(hash);
}

const char* CVarSystemImpl::get_string_cvar(const StringUtils::StringHash hash) {
  OX_SCOPED_ZONE;
  return get_cvar_current<std::string>(hash)->c_str();
}


CVarSystem* CVarSystem::get() {
  static CVarSystemImpl cvarSys{};
  return &cvarSys;
}

CVarParameter* CVarSystemImpl::get_cvar(const StringUtils::StringHash hash) {
  std::shared_lock lock(mutex_);
  const auto it = saved_cvars.find(hash);

  if (it != saved_cvars.end()) {
    return &it->second;
  }

  return nullptr;
}

void CVarSystemImpl::set_float_cvar(const StringUtils::StringHash hash, const float value) {
  set_cvar_current<float>(hash, value);
}

void CVarSystemImpl::set_int_cvar(const StringUtils::StringHash hash, const int32_t value) {
  set_cvar_current<int32_t>(hash, value);
}

void CVarSystemImpl::set_string_cvar(const StringUtils::StringHash hash, const char* value) {
  set_cvar_current<std::string>(hash, value);
}

CVarParameter* CVarSystemImpl::create_float_cvar(const char* name, const char* description, const float defaultValue, const float currentValue) {
  std::unique_lock lock(mutex_);
  CVarParameter* param = init_cvar(name, description);
  if (!param)
    return nullptr;

  param->type = CVarType::FLOAT;

  get_cvar_array<float>()->add(defaultValue, currentValue, param);

  return param;
}

CVarParameter* CVarSystemImpl::create_int_cvar(const char* name, const char* description, const int32_t defaultValue, const int32_t currentValue) {
  std::unique_lock lock(mutex_);
  CVarParameter* param = init_cvar(name, description);
  if (!param)
    return nullptr;

  param->type = CVarType::INT;

  get_cvar_array<int32_t>()->add(defaultValue, currentValue, param);

  return param;
}

CVarParameter* CVarSystemImpl::create_string_cvar(const char* name, const char* description, const char* defaultValue, const char* currentValue) {
  std::unique_lock lock(mutex_);
  CVarParameter* param = init_cvar(name, description);
  if (!param)
    return nullptr;

  param->type = CVarType::STRING;

  get_cvar_array<std::string>()->add(defaultValue, currentValue, param);

  return param;
}

CVarParameter* CVarSystemImpl::init_cvar(const char* name, const char* description) {
  const uint32_t namehash = StringUtils::StringHash{name};
  saved_cvars[namehash] = CVarParameter{};

  CVarParameter& newParam = saved_cvars[namehash];

  newParam.name = name;
  newParam.description = description;

  return &newParam;
}

AutoCVar_Float::AutoCVar_Float(const char* name, const char* description, const float defaultValue, const CVarFlags flags) {
  CVarParameter* cvar = CVarSystem::get()->create_float_cvar(name, description, defaultValue, defaultValue);
  cvar->flags = flags;
  index = cvar->arrayIndex;
}

template <typename T>
T get_cvar_current_by_index(int32_t index) {
  return CVarSystemImpl::get()->get_cvar_array<T>()->get_current(index);
}

template <typename T>
T* ptr_get_cvar_current_by_index(int32_t index) {
  return CVarSystemImpl::get()->get_cvar_array<T>()->get_current_ptr(index);
}


template <typename T>
void set_cvar_current_by_index(int32_t index, const T& data) {
  CVarSystemImpl::get()->get_cvar_array<T>()->set_current(data, index);
}


float AutoCVar_Float::get() const {
  return get_cvar_current_by_index<CVarType>(index);
}

float* AutoCVar_Float::get_ptr() const {
  return ptr_get_cvar_current_by_index<CVarType>(index);
}

float AutoCVar_Float::get_float() const {
  return static_cast<float>(get());
}

float* AutoCVar_Float::get_float_ptr() const {
  auto* result = reinterpret_cast<float*>(get_ptr());
  return result;
}

void AutoCVar_Float::set(const float f) const {
  set_cvar_current_by_index<CVarType>(index, f);
}

AutoCVar_Int::AutoCVar_Int(const char* name, const char* description, const int32_t defaultValue, const CVarFlags flags) {
  CVarParameter* cvar = CVarSystem::get()->create_int_cvar(name, description, defaultValue, defaultValue);
  cvar->flags = flags;
  index = cvar->arrayIndex;
}

int32_t AutoCVar_Int::get() const {
  return get_cvar_current_by_index<CVarType>(index);
}

int32_t* AutoCVar_Int::get_ptr() const {
  return ptr_get_cvar_current_by_index<CVarType>(index);
}

void AutoCVar_Int::set(const int32_t val) const {
  set_cvar_current_by_index<CVarType>(index, val);
}

void AutoCVar_Int::toggle() const {
  const bool enabled = get() != 0;

  set(enabled ? 0 : 1);
}

AutoCVar_String::AutoCVar_String(const char* name, const char* description, const char* defaultValue, const CVarFlags flags) {
  CVarParameter* cvar = CVarSystem::get()->create_string_cvar(name, description, defaultValue, defaultValue);
  cvar->flags = flags;
  index = cvar->arrayIndex;
}

std::string AutoCVar_String::get() const {
  return get_cvar_current_by_index<CVarType>(index);
};

void AutoCVar_String::set(std::string&& val) const {
  set_cvar_current_by_index<CVarType>(index, val);
}
}
