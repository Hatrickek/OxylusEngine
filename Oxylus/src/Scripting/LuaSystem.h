#pragma once
#include <sol/environment.hpp>

#include "Core/Base.h"
#include "Core/Systems/System.h"

namespace Oxylus {
class LuaSystem : public System {
public:
  LuaSystem(const std::string& file_path);
  ~LuaSystem() = default;

  void on_update(Scene* scene, Timestep delta_time) override;
  void on_release(Scene* scene) override;

  void reload();

  const std::string& get_path() const { return m_file_path; }

private:
  std::string m_file_path;
  std::unordered_map<int, std::string> m_errors = {};

  Ref<sol::environment> m_env = nullptr;
  Ref<sol::protected_function> m_on_init_func = nullptr;
  Ref<sol::protected_function> m_on_release_func = nullptr;
  Ref<sol::protected_function> m_on_update_func = nullptr;
  Ref<sol::protected_function> m_on_fixed_update_func = nullptr;

  void init_script(const std::string& file_path);
};
}
