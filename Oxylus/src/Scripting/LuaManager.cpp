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

#define REGISTER_COMPONENT_WITH_ECS(cur_lua_state, Comp, assign_ptr)                                          \
{                                                                                                             \
  using namespace entt;                                                                                       \
  auto entity_type = cur_lua_state["Entity"].get_or_create<sol::usertype<Entity>>();                          \
  entity_type.set_function("add_" #Comp, assign_ptr);                                                         \
  entity_type.set_function("remove_" #Comp, &Entity::remove_component<Comp>);                                 \
  entity_type.set_function("get_" #Comp, &Entity::get_component<Comp>);                                       \
  entity_type.set_function("get_or_add_" #Comp, &Entity::get_or_add_component<Comp>);                         \
  entity_type.set_function("try_get_" #Comp, &Entity::try_get_component<Comp>);                               \
  entity_type.set_function("add_or_replace_" #Comp, &Entity::add_or_replace_component<Comp>);                 \
  entity_type.set_function("has_" #Comp, &Entity::has_component<Comp>);                                       \
  auto entityManager_type = cur_lua_state["enttRegistry"].get_or_create<sol::usertype<registry>>();           \
  entityManager_type.set_function("view_" #Comp, &_ECS_export_view<type_list<Comp>, type_list<>>::view);      \
  auto V = cur_lua_state.new_usertype<view<entt::get_t<Comp>>>(#Comp "_view");                                \
  V.set_function("each", &view<entt::get_t<Comp>>::each<std::function<void(Comp&)>>);                         \
  V.set_function("front", &view<entt::get_t<Comp>>::front);                                                   \
}

void LuaManager::init() {
  m_state = create_ref<sol::state>();
  m_state->open_libraries(sol::lib::base, sol::lib::package, sol::lib::math, sol::lib::table, sol::lib::os, sol::lib::string);

  bind_log();
  bind_math();
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

  sol::usertype<TagComponent> name_component_type = m_state->new_usertype<TagComponent>("NameComponent");
  name_component_type["name"] = &TagComponent::tag;
  REGISTER_COMPONENT_WITH_ECS(state_reference, TagComponent, &Entity::add_component<TagComponent>)

  sol::usertype<TransformComponent> transform_component_type = m_state->new_usertype<TransformComponent>("TransformComponent");
  transform_component_type["position"] = &TransformComponent::position;
  transform_component_type["rotation"] = &TransformComponent::rotation;
  transform_component_type["scale"] = &TransformComponent::scale;
  REGISTER_COMPONENT_WITH_ECS(state_reference, TransformComponent, &Entity::add_component<TransformComponent>)
}

#define SET_MATH_FUNCTIONS(var, type)                                                                            \
{                                                                                                                \
  (var).set_function(sol::meta_function::addition, [](const type& a, const type& b) { return a + b; });          \
  (var).set_function(sol::meta_function::multiplication, [](const type& a, const type& b) { return a * b; });    \
  (var).set_function(sol::meta_function::subtraction, [](const type& a, const type& b) { return a - b; });       \
  (var).set_function(sol::meta_function::division, [](const type& a, const type& b) { return a / b; });          \
  (var).set_function(sol::meta_function::equal_to, [](const type& a, const type& b) { return a == b; });         \
  (var).set_function(sol::meta_function::unary_minus, [](const type& v) -> type { return -v; });                 \
}

void LuaManager::bind_math() {
  auto vec2 = m_state->new_usertype<Vec2>("Vec2", sol::constructors<Vec2(float, float)>());
  SET_MATH_FUNCTIONS(vec2, Vec2)
  vec2["x"] = &Vec2::x;
  vec2["y"] = &Vec2::y;

  auto mult_overloads_vec3 = sol::overload(
    [](const Vec3& v1, const Vec3& v2) -> Vec3 { return v1 * v2; },
    [](const Vec3& v1, const float f) -> Vec3 { return v1 * f; },
    [](const float f, const Vec3& v1) -> Vec3 { return f * v1; });

  auto vec3 = m_state->new_usertype<Vec3>("Vec3", sol::constructors<sol::types<>, sol::types<float, float, float>>());
  vec3["x"] = &Vec3::x;
  vec3["y"] = &Vec3::y;
  vec3["z"] = &Vec3::z;
  SET_MATH_FUNCTIONS(vec3, Vec3)
  vec3.set_function("length", [](const Vec3& v) { return glm::length(v); });
  vec3.set_function("distance", [](const Vec3& a, const Vec3& b) { return distance(a, b); });
  vec3.set_function("normalize", [](const Vec3& a) { return normalize(a); });
  vec3.set_function(sol::meta_function::multiplication, mult_overloads_vec3);

  auto mult_overloads_vec4 = sol::overload(
    [](const Vec3& v1, const Vec3& v2) -> Vec3 { return v1 * v2; },
    [](const Vec3& v1, const float f) -> Vec3 { return v1 * f; },
    [](const float f, const Vec3& v1) -> Vec3 { return f * v1; });

  auto vec4 = m_state->new_usertype<Vec4>("Vec4", sol::constructors<sol::types<>, sol::types<float, float, float, float>>());
  vec4["x"] = &Vec4::x;
  vec4["y"] = &Vec4::y;
  vec4["z"] = &Vec4::z;
  vec4["w"] = &Vec4::w;
  SET_MATH_FUNCTIONS(vec4, Vec4)
  vec4.set_function(sol::meta_function::multiplication, mult_overloads_vec4);
  vec4.set_function("length", [](const Vec4& v) { return glm::length(v); });
  vec4.set_function("distance", [](const Vec4& a, const Vec4& b) { return distance(a, b); });
  vec4.set_function("normalize", [](const Vec4& a) { return normalize(a); });
}
}
