#include "LuaDebugBindings.h"

#include <sol/state.hpp>
#include "Sol2Helpers.h"

#include "Physics/RayCast.h"

#include "Render/DebugRenderer.h"

namespace Ox::LuaBindings {
void bind_debug_renderer(const Shared<sol::state>& state) {
  auto debug_table = state->create_table("Debug");
  debug_table.set_function("draw_point",
                           [](const Vec3& point) -> void {
                             DebugRenderer::draw_point(point, 1.0f);
                           });
  debug_table.set_function("draw_line",
                           [](const Vec3& start, const Vec3& end, const Vec3& color = Vec3(1)) -> void {
                             DebugRenderer::draw_line(start, end, 1.0f, Vec4(color, 1.0f));
                           });
  debug_table.set_function("draw_ray",
                           [](const RayCast& ray, const Vec3& color = Vec3(1)) -> void {
                             DebugRenderer::draw_line(ray.get_origin(), ray.get_direction(), 1.0f, Vec4(color, 1.0f));
                           });
  // TODO: bind AABB type
  debug_table.set_function("draw_aabb",
                           [](const AABB& aabb, const Vec3& color = Vec3(1)) -> void {
                             DebugRenderer::draw_aabb(aabb, Vec4(color, 1.0f));
                           });
}
}
