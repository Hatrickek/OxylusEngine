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
  if (!std::filesystem::exists(file_path)) {
    OX_CORE_ERROR("Couldn't find the script file!");
    return;
  }

  const auto state = LuaManager::get()->get_state();
  m_env = create_ref<sol::environment>(*state, sol::create, state->globals());

  const auto loadFileResult = state->script_file(file_path, *m_env, sol::script_pass_on_error);

  if (!loadFileResult.valid()) {
    const sol::error err = loadFileResult;
    OX_CORE_ERROR("Failed to Execute Lua script {0}", file_path);
    OX_CORE_ERROR("Error : {0}", err.what());
    std::string error = std::string(err.what());

    const auto linepos = error.find(".lua:");
    std::string errorLine = error.substr(linepos + 5); //+4 .lua: + 1
    const auto lineposEnd = errorLine.find(':');
    errorLine = errorLine.substr(0, lineposEnd);
    const int line = std::stoi(errorLine);
    error = error.substr(linepos + errorLine.size() + lineposEnd + 4); //+4 .lua:

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
  if (m_on_update_func) {
    (*m_env)["scene"] = scene;
    (*m_env)["current_time"] = delta_time.get_elapsed_seconds();
    const auto result = m_on_update_func->call(scene, delta_time.get_millis());
    check_result(result);
  }
}

void LuaSystem::on_release(Scene* scene) {
  if (m_on_release_func) {
    const auto result = m_on_release_func->call(scene);
    check_result(result);
  }
}

void LuaSystem::reload() {
  if (m_env) {
    const sol::protected_function releaseFunc = (*m_env)["on_release"];
    if (releaseFunc.valid())
      releaseFunc.call();
  }

  init_script(m_file_path);
}
}
