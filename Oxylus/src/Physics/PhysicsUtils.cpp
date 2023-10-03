#include "PhysicsUtils.h"

#include <glm/gtc/type_ptr.hpp>

#include "Render/DebugRenderer.h"

namespace Oxylus {
void PhysicsUtils::DebugDraw(Scene* scene, entt::entity entity) {
  const auto e = Entity{entity, scene};
  const auto tc = e.GetTransform();
  if (e.HasComponent<BoxColliderComponent>()) {
    DebugRenderer::draw_box(tc.translation,
      e.GetComponent<BoxColliderComponent>().size * 2.0f,
      Vec4(0, 1, 0, 1),
      tc.rotation);
  }
  if (e.HasComponent<SphereColliderComponent>()) {
    DebugRenderer::draw_sphere(tc.translation,
      Vec3(e.GetComponent<SphereColliderComponent>().radius * 2.0f),
      Vec4(0, 1, 0, 1),
      tc.rotation);
  }
  if (e.HasComponent<CharacterControllerComponent>()) {
    const auto& ch = e.GetComponent<CharacterControllerComponent>();
    DebugRenderer::draw_sphere(tc.translation,
      Vec3(ch.character_radius_standing, 0.5f * ch.character_height_standing, ch.character_radius_standing),
      Vec4(0, 1, 0, 1),
      tc.rotation);
  }
}
}
