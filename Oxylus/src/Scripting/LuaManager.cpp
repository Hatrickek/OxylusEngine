#include "LuaManager.h"

#include <sol/sol.hpp>

#include "Scene/Scene.h"
#include "Core/Entity.h"

namespace Oxylus {
template <typename, typename>
struct _ECS_export_view;

template <typename... Component, typename... Exclude>
struct _ECS_export_view<entt::type_list<Component...>, entt::type_list<Exclude...>> {
  static entt::view<entt::get_t<Component...>> view(entt::registry& registry) {
    return registry.view<Component...>(entt::exclude<Exclude...>);
  }
};

#define REGISTER_COMPONENT_WITH_ECS(curLuaState, Comp, assignPtr)                                             \
{                                                                                                             \
  using namespace entt;                                                                                       \
  auto entity_type = curLuaState["Entity"].get_or_create<sol::usertype<Entity>>();                            \
  entity_type.set_function("add_" #Comp, assignPtr);                                                          \
  entity_type.set_function("remove_" #Comp, &Entity::remove_component<Comp>);                                 \
  entity_type.set_function("get_" #Comp, &Entity::get_component<Comp>);                                       \
  entity_type.set_function("get_or_add_" #Comp, &Entity::get_or_add_component<Comp>);                         \
  entity_type.set_function("try_get_" #Comp, &Entity::try_get_component<Comp>);                               \
  entity_type.set_function("add_or_replace_" #Comp, &Entity::add_or_replace_component<Comp>);                 \
  entity_type.set_function("has_" #Comp, &Entity::has_component<Comp>);                                       \
  auto entityManager_type = curLuaState["enttRegistry"].get_or_create<sol::usertype<registry>>();             \
  entityManager_type.set_function("view_" #Comp, &_ECS_export_view<type_list<Comp>, type_list<>>::view);      \
  auto V = curLuaState.new_usertype<view<entt::get_t<Comp>>>(#Comp "_view");                                  \
  V.set_function("each", &view<entt::get_t<Comp>>::each<std::function<void(Comp&)>>);                         \
  V.set_function("front", &view<entt::get_t<Comp>>::front);                                                   \
}

void LuaManager::init() {
  m_state = create_ref<sol::state>();
  m_state->open_libraries(sol::lib::base, sol::lib::package, sol::lib::math, sol::lib::table, sol::lib::os, sol::lib::string);

  bind_log();
  bind_ecs();
}

LuaManager* LuaManager::get() {
  static LuaManager manager = {};
  return &manager;
}

void LuaManager::bind_log() const {
  auto log = m_state->create_table("log");

  log.set_function("trace", [&](sol::this_state s, std::string_view message) { OX_CORE_TRACE(message); });
  log.set_function("info", [&](sol::this_state s, std::string_view message) { OX_CORE_INFO(message); });
  log.set_function("warn", [&](sol::this_state s, std::string_view message) { OX_CORE_WARN(message); });
  log.set_function("error", [&](sol::this_state s, std::string_view message) { OX_CORE_ERROR(message); });
  log.set_function("fatal", [&](sol::this_state s, std::string_view message) { OX_CORE_FATAL(message); });
}

void LuaManager::bind_ecs() {
  sol::state& state_reference = *m_state.get();

  sol::usertype<entt::registry> enttRegistry = m_state->new_usertype<entt::registry>("EnttRegistry");
  sol::usertype<Entity> entityType = m_state->new_usertype<Entity>("Entity", sol::constructors<sol::types<entt::entity, Scene*>>());

  sol::usertype<Scene> scene_type = m_state->new_usertype<Scene>("Scene");
  scene_type.set_function("get_registery", &Scene::get_registry);

  scene_type.set_function("create_entity", &Scene::create_entity);

  sol::usertype<TagComponent> nameComponent_type = m_state->new_usertype<TagComponent>("NameComponent");
  nameComponent_type["name"] = &TagComponent::tag;
  REGISTER_COMPONENT_WITH_ECS(state_reference, TagComponent, &Entity::add_component<TagComponent>)
}
}
