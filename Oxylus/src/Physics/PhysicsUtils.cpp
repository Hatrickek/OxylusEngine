#include "PhysicsUtils.h"

#include <glm/gtc/type_ptr.hpp>

#include "Render/DebugRenderer.h"

namespace Oxylus {
void PhysicsUtils::DebugDraw(Scene* scene, entt::entity entity) {
  const auto e = Entity{entity, scene};
  const auto tc = e.get_transform();
  if (e.has_component<BoxColliderComponent>()) {
    DebugRenderer::draw_box(tc.translation,
      e.get_component<BoxColliderComponent>().size * 2.0f,
      Vec4(0, 1, 0, 1),
      tc.rotation);
  }
  if (e.has_component<SphereColliderComponent>()) {
    DebugRenderer::draw_sphere(tc.translation,
      Vec3(e.get_component<SphereColliderComponent>().radius * 2.0f),
      Vec4(0, 1, 0, 1),
      tc.rotation);
  }
  if (e.has_component<CharacterControllerComponent>()) {
    const auto& ch = e.get_component<CharacterControllerComponent>();
    DebugRenderer::draw_sphere(tc.translation,
      Vec3(ch.character_radius_standing, 0.5f * ch.character_height_standing, ch.character_radius_standing),
      Vec4(0, 1, 0, 1),
      tc.rotation);
  }
}
}
