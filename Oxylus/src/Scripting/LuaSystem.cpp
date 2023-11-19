#include "LuaSystem.h"

#include <filesystem>

#include <sol/state.hpp>

#include "LuaManager.h"

#include "Scene/Scene.h"

#include "Utils/FileUtils.h"
#include "Utils/Log.h"

namespace Oxylus {
static void check_result(const sol::protected_function_result& result) {
  if (!result.valid()) {
    const sol::error err = result;
    OX_CORE_ERROR("Failed to Execute Lua Script on_update");
    OX_CORE_ERROR("Error : {0}", err.what());
  }
}

LuaSystem::LuaSystem(const std::string& file_path) : m_file_path(file_path) {
  init_script(file_path);
}

void LuaSystem::init_script(const std::string& file_path) {
  OX_SCOPED_ZONE;
  if (!std::filesystem::exists(file_path)) {
    OX_CORE_ERROR("Couldn't find the script file!");
    return;
  }

  const auto state = LuaManager::get()->get_state();
  m_env = create_ref<sol::environment>(*state, sol::create, state->globals());

  const auto load_file_result = state->script_file(file_path, *m_env, sol::script_pass_on_error);

  if (!load_file_result.valid()) {
    const sol::error err = load_file_result;
    OX_CORE_ERROR("Failed to Execute Lua script {0}", file_path);
    OX_CORE_ERROR("Error : {0}", err.what());
    std::string error = std::string(err.what());

    const auto linepos = error.find(".lua:");
    std::string error_line = error.substr(linepos + 5); //+4 .lua: + 1
    const auto linepos_end = error_line.find(':');
    error_line = error_line.substr(0, linepos_end);
    const int line = std::stoi(error_line);
    error = error.substr(linepos + error_line.size() + linepos_end + 4); //+4 .lua:

    m_errors[line] = error;
  }

  for (auto [l, e] : m_errors) {
    OX_CORE_ERROR("{} {}", l, e);
  }

  m_on_update_func = create_ref<sol::protected_function>((*m_env)["on_update"]);
  if (!m_on_update_func->valid())
    m_on_update_func.reset();

  m_on_release_func = create_ref<sol::protected_function>((*m_env)["on_release"]);
  if (!m_on_release_func->valid())
    m_on_release_func.reset();

  state->collect_garbage();
}

void LuaSystem::on_update(Scene* scene, const Timestep& delta_time) {
  OX_SCOPED_ZONE;
  if (m_on_update_func) {
    (*m_env)["scene"] = scene;
    (*m_env)["current_time"] = delta_time.get_elapsed_seconds();
    const auto result = m_on_update_func->call(scene, delta_time.get_millis());
    check_result(result);
  }
}

void LuaSystem::on_release(Scene* scene) {
  OX_SCOPED_ZONE;
  if (m_on_release_func) {
    const auto result = m_on_release_func->call(scene);
    check_result(result);
  }
}

void LuaSystem::load(const std::string& file_path) {
  OX_SCOPED_ZONE;
  init_script(file_path);
}

void LuaSystem::reload() {
  OX_SCOPED_ZONE;
  if (m_env) {
    const sol::protected_function releaseFunc = (*m_env)["on_release"];
    if (releaseFunc.valid())
      releaseFunc.call();
  }

  init_script(m_file_path);
}
}
