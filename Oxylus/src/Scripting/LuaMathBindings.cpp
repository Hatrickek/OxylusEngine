#include "LuaMathBindings.h"

#include <sol/overload.hpp>
#include <sol/state.hpp>
#include <sol/types.hpp>

#include "LuaHelpers.h"

#include "Core/Types.h"

#include "Render/BoundingVolume.h"

#include "Utils/Profiler.h"

namespace Ox::LuaBindings {
#define SET_MULTIPLICATION_OVERLOADS(var, type) \
  (var).set_function(sol::meta_function::multiplication, sol::overload([](const type& a, const type& b) { return a * b; }, \
                                                                       [](const type& a, const float b) { return a * b; }, \
                                                                       [](const float a, const type& b) { return a * b; }));
#define SET_SUBTRACTION_OVERLOADS(var, type) \
(var).set_function(sol::meta_function::subtraction, sol::overload([](const type& a, const type& b) { return a - b; }, \
                                                                  [](const type& a, const float b) { return a - b; }, \
                                                                  [](const float a, const type& b) { return a - b; }));

#define SET_ADDITION_OVERLOADS(var, type) \
(var).set_function(sol::meta_function::addition, sol::overload([](const type& a, const type& b) { return a + b; }, \
                                                                  [](const type& a, const float b) { return a + b; }, \
                                                                  [](const float a, const type& b) { return a + b; }));

#define SET_MATH_META_FUNCTIONS(var, type) \
  (var).set_function(sol::meta_function::division, [](const type& a, const type& b) { return a / b; });          \
  (var).set_function(sol::meta_function::equal_to, [](const type& a, const type& b) { return a == b; });         \
  (var).set_function(sol::meta_function::unary_minus, [](const type& v) -> type { return -v; });
#define SET_MATH_FUNCTIONS(var, type)                                                                            \
  SET_MATH_META_FUNCTIONS(var, type)\
  SET_MULTIPLICATION_OVERLOADS(var, type) \
  SET_SUBTRACTION_OVERLOADS(var, type) \
  SET_ADDITION_OVERLOADS(var, type) \
  (var).set_function("length", [](const type& v) -> float { return glm::length(v); });                 \
  (var).set_function("distance", [](const type& a, const type& b) -> float { return glm::distance(a, b); });                 \
  (var).set_function("normalize", [](const type& v) -> type { return glm::normalize(v); });

void bind_math(const Shared<sol::state>& state) {
  OX_SCOPED_ZONE;
  auto vec2 = state->new_usertype<Vec2>("Vec2", sol::constructors<Vec2(float, float)>());
  SET_MATH_FUNCTIONS(vec2, Vec2)
  SET_TYPE_FIELD(vec2, Vec2, x);
  SET_TYPE_FIELD(vec2, Vec2, y);

  auto uvec2 = state->new_usertype<UVec2>("UVec2", sol::constructors<UVec2(unsigned int, unsigned int)>());
  SET_MATH_META_FUNCTIONS(uvec2, UVec2)
  SET_TYPE_FIELD(uvec2, UVec2, x);
  SET_TYPE_FIELD(uvec2, UVec2, y);

  auto mult_overloads_vec3 = sol::overload(
    [](const Vec3& v1, const Vec3& v2) -> Vec3 { return v1 * v2; },
    [](const Vec3& v1, const float f) -> Vec3 { return v1 * f; },
    [](const float f, const Vec3& v1) -> Vec3 { return f * v1; });

  auto vec3 = state->new_usertype<Vec3>("Vec3", sol::constructors<sol::types<>, sol::types<float, float, float>>());
  SET_TYPE_FIELD(vec3, Vec3, x);
  SET_TYPE_FIELD(vec3, Vec3, y);
  SET_TYPE_FIELD(vec3, Vec3, z);
  SET_MATH_FUNCTIONS(vec3, Vec3)
  vec3.set_function(sol::meta_function::multiplication, mult_overloads_vec3);

  auto mult_overloads_vec4 = sol::overload(
    [](const Vec3& v1, const Vec3& v2) -> Vec3 { return v1 * v2; },
    [](const Vec3& v1, const float f) -> Vec3 { return v1 * f; },
    [](const float f, const Vec3& v1) -> Vec3 { return f * v1; });

  auto vec4 = state->new_usertype<Vec4>("Vec4", sol::constructors<sol::types<>, sol::types<float, float, float, float>>());
  SET_TYPE_FIELD(vec4, Vec4, x);
  SET_TYPE_FIELD(vec4, Vec4, y);
  SET_TYPE_FIELD(vec4, Vec4, z);
  SET_TYPE_FIELD(vec4, Vec4, w);
  SET_MATH_FUNCTIONS(vec4, Vec4)
  vec4.set_function(sol::meta_function::multiplication, mult_overloads_vec4);

  state->new_usertype<Mat3>("Mat3",
                            sol::constructors<Mat3(float, float, float, float, float, float, float, float, float), Mat3(float), Mat3()>(),
                            sol::meta_function::multiplication,
                            [](const Mat3& a, const Mat3& b) { return a * b; });

  state->new_usertype<Mat4>("Mat4",
                            sol::constructors<Mat4(float), Mat4()>(),
                            sol::meta_function::multiplication,
                            [](const Mat4& a, const Mat4& b) { return a * b; },
                            sol::meta_function::addition,
                            [](const Mat4& a, const Mat4& b) { return a + b; },
                            sol::meta_function::subtraction,
                            [](const Mat4& a, const Mat4& b) { return a - b; });

  auto aabb = state->new_usertype<AABB>("AABB", sol::constructors<Vec3(), Vec3()>());
  SET_TYPE_FUNCTION(aabb, AABB, get_center);
  SET_TYPE_FUNCTION(aabb, AABB, get_extents);
  SET_TYPE_FUNCTION(aabb, AABB, get_size);
  SET_TYPE_FUNCTION(aabb, AABB, translate);
  SET_TYPE_FUNCTION(aabb, AABB, scale);
  SET_TYPE_FUNCTION(aabb, AABB, transform);
  SET_TYPE_FUNCTION(aabb, AABB, get_transformed);
  SET_TYPE_FUNCTION(aabb, AABB, merge);
  // TODO: Bind frustum and Plane
  //SET_TYPE_FUNCTION(aabb, AABB, is_on_frustum);
  //SET_TYPE_FUNCTION(aabb, AABB, is_on_or_forward_plane);

  const std::initializer_list<std::pair<std::string_view, Intersection>> intersection = {
    {"Outside", Outside},
    {"Intersects", Intersects},
    {"Inside", Inside}
  };
  state->new_enum("Intersection", intersection);
  SET_TYPE_FUNCTION(aabb, AABB, is_aabb_inside);
  SET_TYPE_FUNCTION(aabb, AABB, is_aabb_inside_fast);
  SET_TYPE_FUNCTION(aabb, AABB, is_point_inside);
}
}
