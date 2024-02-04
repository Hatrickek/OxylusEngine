#include "LuaMathBindings.h"

#include <sol/overload.hpp>
#include <sol/state.hpp>
#include <sol/types.hpp>

#include "Core/Types.h"

#include "Utils/Profiler.h"

namespace Oxylus::LuaBindings {
#define SET_MATH_FUNCTIONS(var, type)                                                                            \
{                                                                                                                \
  (var).set_function(sol::meta_function::addition, [](const type& a, const type& b) { return a + b; });          \
  (var).set_function(sol::meta_function::multiplication, [](const type& a, const type& b) { return a * b; });    \
  (var).set_function(sol::meta_function::subtraction, [](const type& a, const type& b) { return a - b; });       \
  (var).set_function(sol::meta_function::division, [](const type& a, const type& b) { return a / b; });          \
  (var).set_function(sol::meta_function::equal_to, [](const type& a, const type& b) { return a == b; });         \
  (var).set_function(sol::meta_function::unary_minus, [](const type& v) -> type { return -v; });                 \
}

void bind_math(const Shared<sol::state>& state) {
  OX_SCOPED_ZONE;
  auto vec2 = state->new_usertype<Vec2>("Vec2", sol::constructors<Vec2(float, float)>());
  SET_MATH_FUNCTIONS(vec2, Vec2)
  vec2["x"] = &Vec2::x;
  vec2["y"] = &Vec2::y;

  auto mult_overloads_vec3 = sol::overload(
    [](const Vec3& v1, const Vec3& v2) -> Vec3 { return v1 * v2; },
    [](const Vec3& v1, const float f) -> Vec3 { return v1 * f; },
    [](const float f, const Vec3& v1) -> Vec3 { return f * v1; });

  auto vec3 = state->new_usertype<Vec3>("Vec3", sol::constructors<sol::types<>, sol::types<float, float, float>>());
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

  auto vec4 = state->new_usertype<Vec4>("Vec4", sol::constructors<sol::types<>, sol::types<float, float, float, float>>());
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
