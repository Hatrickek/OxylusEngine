#include "LuaSceneBindings.h"

#include <sol/state.hpp>

#include "Sol2Helpers.h"

#include "Scene/Entity.h"

namespace Ox::LuaBindings {
void bind_scene(const Shared<sol::state>& state) {
  OX_SCOPED_ZONE;
  state->new_usertype<entt::registry>("EnttRegistry");
  state->new_usertype<Entity>("Entity", sol::constructors<sol::types<entt::entity, Scene*>>());

  sol::usertype<Scene> scene_type = state->new_usertype<Scene>("Scene");
  scene_type.set_function("get_registery", &Scene::get_registry);
  scene_type.set_function("create_entity", &Scene::create_entity);
  scene_type.set_function("load_mesh", &Scene::load_mesh);
}
}
