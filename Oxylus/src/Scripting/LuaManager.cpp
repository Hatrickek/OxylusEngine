#include "LuaManager.h"

#include <sol/sol.hpp>

#include "LuaApplicationBindings.h"
#include "LuaAssetManagerBindings.h"
#include "LuaAudioBindings.h"
#include "LuaComponentBindings.h"
#include "LuaDebugBindings.h"
#include "LuaSceneBindings.h"
#include "LuaInputBindings.h"
#include "LuaMathBindings.h"
#include "LuaPhysicsBindings.h"
#include "LuaRendererBindings.h"
#include "LuaUIBindings.h"

#include "Scene/Scene.h"
#include "Scene/Entity.h"
#include "Core/Input.h"

namespace Ox {
void LuaManager::init() {
  OX_SCOPED_ZONE;
  m_state = create_shared<sol::state>();
  m_state->open_libraries(sol::lib::base, sol::lib::package, sol::lib::math, sol::lib::table, sol::lib::os, sol::lib::string);

  bind_log();
  LuaBindings::bind_application(m_state);
  LuaBindings::bind_math(m_state);
  LuaBindings::bind_scene(m_state);
  LuaBindings::bind_asset_manager(m_state);
  LuaBindings::bind_debug_renderer(m_state);
  LuaBindings::bind_renderer(m_state);
  LuaBindings::bind_components(m_state);
  LuaBindings::bind_input(m_state);
  LuaBindings::bind_audio(m_state);
  LuaBindings::bind_physics(m_state);
  LuaBindings::bind_ui(m_state);
}

void LuaManager::shutdown() {
  m_state->collect_gc();
  m_state.reset();
}

LuaManager* LuaManager::get() {
  static LuaManager manager = {};
  return &manager;
}

#define SET_LOG_FUNCTIONS(table, name, log_func) \
  table.set_function(name, sol::overload([](const std::string_view message) { log_func("{}", message);}, \
                                         [](const Vec4& vec4) { log_func("x: {} y: {} z: {} w: {}", vec4.x, vec4.y, vec4.z, vec4.w); }, \
                                         [](const Vec3& vec3) { log_func("x: {} y: {} z: {}", vec3.x, vec3.y, vec3.z); }, \
                                         [](const Vec2& vec2) { log_func("x: {} y: {}", vec2.x, vec2.y); }, \
                                         [](const UVec2& vec2) { log_func("x: {} y: {}", vec2.x, vec2.y); } \
));

void LuaManager::bind_log() const {
  OX_SCOPED_ZONE;
  auto log = m_state->create_table("Log");

  SET_LOG_FUNCTIONS(log, "trace", OX_CORE_TRACE)
  SET_LOG_FUNCTIONS(log, "info", OX_CORE_INFO)
  SET_LOG_FUNCTIONS(log, "warn", OX_CORE_WARN)
  SET_LOG_FUNCTIONS(log, "error", OX_CORE_ERROR)
}
}
