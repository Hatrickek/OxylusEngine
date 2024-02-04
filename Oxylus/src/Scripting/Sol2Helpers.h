#pragma once
#include <entt/entt.hpp>

template <typename, typename>
struct _ECS_export_view;

template <typename... Component, typename... Exclude>
struct _ECS_export_view<entt::type_list<Component...>, entt::type_list<Exclude...>> {
  static entt::view<entt::get_t<Component...>> view(entt::registry& registry) {
    return registry.view<Component...>(entt::exclude<Exclude...>);
  }
};

#define REGISTER_COMPONENT_WITH_ECS(cur_lua_state, Comp)                                          \
{                                                                                                             \
  using namespace entt;                                                                                       \
  auto entity_type = (*cur_lua_state)["Entity"].get_or_create<sol::usertype<Entity>>();                          \
  entity_type.set_function("add_" #Comp, &Entity::add_component<Comp>);                                                         \
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

#define SET_TYPE_FIELD(var, type, field) var[#field] = &type::field
#define SET_TYPE_FUNCTION(var, type, function) var.set_function(#function, &type::function)
