#include "Utils/CVars.h"

#include <mutex>
#include <shared_mutex>

#include <ankerl/unordered_dense.h>

#include "Log.h"
#include "Profiler.h"

namespace Ox {
float* CVarSystem::get_float_cvar(const StringUtils::StringHash hash) {
  OX_SCOPED_ZONE;
  return get_cvar_current<float>(hash);
}

int32_t* CVarSystem::get_int_cvar(const StringUtils::StringHash hash) {
  OX_SCOPED_ZONE;
  return get_cvar_current<int32_t>(hash);
}

const char* CVarSystem::get_string_cvar(const StringUtils::StringHash hash) {
  OX_SCOPED_ZONE;
  return get_cvar_current<std::string>(hash)->c_str();
}


CVarSystem* CVarSystem::get() {
  static CVarSystem cvar_sys{};
  return &cvar_sys;
}

CVarParameter* CVarSystem::get_cvar(const StringUtils::StringHash hash) {
  std::shared_lock lock(mutex_);

  if (saved_cvars.contains(hash))
    return &saved_cvars[hash];

  return nullptr;
}

void CVarSystem::set_float_cvar(const StringUtils::StringHash hash, const float value) {
  set_cvar_current<float>(hash, value);
}

void CVarSystem::set_int_cvar(const StringUtils::StringHash hash, const int32_t value) {
  set_cvar_current<int32_t>(hash, value);
}

void CVarSystem::set_string_cvar(const StringUtils::StringHash hash, const char* value) {
  set_cvar_current<std::string>(hash, value);
}

CVarParameter* CVarSystem::create_float_cvar(const char* name, const char* description, const float default_value, const float current_value) {
  std::unique_lock lock(mutex_);
  CVarParameter* param = init_cvar(name, description);

  param->type = CVarType::FLOAT;
  param->array_index = (uint32_t)get_cvar_array<float>()->size();
  get_cvar_array<float>()->emplace_back(default_value, current_value, param);

  return param;
}

CVarParameter* CVarSystem::create_int_cvar(const char* name, const char* description, const int32_t default_value, const int32_t current_value) {
  std::unique_lock lock(mutex_);
  CVarParameter* param = init_cvar(name, description);

  param->type = CVarType::INT;
  param->array_index = (uint32_t)get_cvar_array<int32_t>()->size();
  get_cvar_array<int32_t>()->emplace_back(default_value, current_value, param);

  return param;
}

CVarParameter* CVarSystem::create_string_cvar(const char* name, const char* description, const char* default_value, const char* current_value) {
  std::unique_lock lock(mutex_);
  CVarParameter* param = init_cvar(name, description);

  param->type = CVarType::STRING;
  param->array_index = (uint32_t)get_cvar_array<std::string>()->size();
  get_cvar_array<std::string>()->emplace_back(default_value, current_value, param);

  return param;
}

CVarParameter* CVarSystem::init_cvar(const char* name, const char* description) {
  const uint32_t namehash = StringUtils::StringHash{name};
  saved_cvars.emplace(namehash, CVarParameter{.name = std::string(name), .description = std::string(description)});

  return &saved_cvars[namehash];
}

AutoCVar_Float::AutoCVar_Float(const char* name, const char* description, const float default_value, const CVarFlags flags) {
  CVarParameter* cvar = CVarSystem::get()->create_float_cvar(name, description, default_value, default_value);
  cvar->flags = flags;
  index = cvar->array_index;
}

template <typename T>
T get_cvar_current_by_index(uint32_t index) {
  return CVarSystem::get()->get_cvar_array<T>()->at(index).current;
}

template <typename T>
T* ptr_get_cvar_current_by_index(uint32_t index) {
  return &CVarSystem::get()->get_cvar_array<T>()->at(index).current;
}

template <typename T>
void set_cvar_current_by_index(uint32_t index, const T& data) {
  CVarSystem::get()->get_cvar_array<T>()->at(index).current = data;
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

AutoCVar_Int::AutoCVar_Int(const char* name, const char* description, const int32_t default_value, const CVarFlags flags) {
  CVarParameter* cvar = CVarSystem::get()->create_int_cvar(name, description, default_value, default_value);
  cvar->flags = flags;
  index = cvar->array_index;
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

AutoCVar_String::AutoCVar_String(const char* name, const char* description, const char* default_value, const CVarFlags flags) {
  CVarParameter* cvar = CVarSystem::get()->create_string_cvar(name, description, default_value, default_value);
  cvar->flags = flags;
  index = cvar->array_index;
}

std::string AutoCVar_String::get() const {
  return get_cvar_current_by_index<CVarType>(index);
};

void AutoCVar_String::set(std::string&& val) const {
  set_cvar_current_by_index<CVarType>(index, val);
}
}
