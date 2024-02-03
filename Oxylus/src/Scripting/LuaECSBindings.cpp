#include "LuaECSBindings.h"

#include <sol/state.hpp>

#include "Scene/Entity.h"

namespace Oxylus::LuaBindings {
template <typename, typename>
struct _ECS_export_view;

template <typename... Component, typename... Exclude>
struct _ECS_export_view<entt::type_list<Component...>, entt::type_list<Exclude...>> {
  static entt::view<entt::get_t<Component...>> view(entt::registry& registry) {
    return registry.view<Component...>(entt::exclude<Exclude...>);
  }
};

#define REGISTER_COMPONENT_WITH_ECS(cur_lua_state, Comp, assign_ptr)                                          \
{                                                                                                             \
  using namespace entt;                                                                                       \
  auto entity_type = (*cur_lua_state)["Entity"].get_or_create<sol::usertype<Entity>>();                          \
  entity_type.set_function("add_" #Comp, assign_ptr);                                                         \
  entity_type.set_function("remove_" #Comp, &Entity::remove_component<Comp>);                                 \
  entity_type.set_function("get_" #Comp, &Entity::get_component<Comp>);                                       \
  entity_type.set_function("get_or_add_" #Comp, &Entity::get_or_add_component<Comp>);                         \
  entity_type.set_function("try_get_" #Comp, &Entity::try_get_component<Comp>);                               \
  entity_type.set_function("add_or_replace_" #Comp, &Entity::add_or_replace_component<Comp>);                 \
  entity_type.set_function("has_" #Comp, &Entity::has_component<Comp>);                                       \
  auto entityManager_type = (*cur_lua_state)["enttRegistry"].get_or_create<sol::usertype<registry>>();           \
  entityManager_type.set_function("view_" #Comp, &_ECS_export_view<type_list<Comp>, type_list<>>::view);      \
  auto V = (*cur_lua_state).new_usertype<view<entt::get_t<Comp>>>(#Comp "_view");                                \
  V.set_function("each", &view<entt::get_t<Comp>>::each<std::function<void(Comp&)>>);                         \
  V.set_function("front", &view<entt::get_t<Comp>>::front);                                                   \
}

void bind_lua_ecs(const Shared<sol::state>& state) {
  OX_SCOPED_ZONE;
  state->new_usertype<entt::registry>("EnttRegistry");
  state->new_usertype<Entity>("Entity", sol::constructors<sol::types<entt::entity, Scene*>>());

  sol::usertype<Scene> scene_type = state->new_usertype<Scene>("Scene");
  scene_type.set_function("get_registery", &Scene::get_registry);

  scene_type.set_function("create_entity", &Scene::create_entity);

  sol::usertype<TagComponent> name_component_type = state->new_usertype<TagComponent>("NameComponent");
  name_component_type["name"] = &TagComponent::tag;
  REGISTER_COMPONENT_WITH_ECS(state, TagComponent, &Entity::add_component<TagComponent>)

  sol::usertype<TransformComponent> transform_component_type = state->new_usertype<TransformComponent>("TransformComponent");
  transform_component_type["position"] = &TransformComponent::position;
  transform_component_type["rotation"] = &TransformComponent::rotation;
  transform_component_type["scale"] = &TransformComponent::scale;
  REGISTER_COMPONENT_WITH_ECS(state, TransformComponent, &Entity::add_component<TransformComponent>)

  sol::usertype<LightComponent> light_component_type = state->new_usertype<LightComponent>("LightComponent");
  light_component_type["color"] = &LightComponent::color;
  light_component_type["intensity"] = &LightComponent::intensity;
  REGISTER_COMPONENT_WITH_ECS(state, LightComponent, &Entity::add_component<LightComponent>)
}
}
