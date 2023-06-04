#include "oxpch.h"
#include "PhysicsUtils.h"

#include <glm/gtc/type_ptr.hpp>

#include "Render/DebugRenderer.h"

namespace Oxylus {
  void PhysicsUtils::DebugDraw(Scene* scene, entt::entity entity) {
    const auto e = Entity{entity, scene};
    const auto tc = e.GetTransform();
    if (e.HasComponent<BoxColliderComponent>()) {
      DebugRenderer::DrawBox(tc.Translation,
        e.GetComponent<BoxColliderComponent>().Size * 2.0f,
        Vec4(0, 1, 0, 1),
        tc.Rotation);
    }
    if (e.HasComponent<SphereColliderComponent>()) {
        DebugRenderer::DrawSphere(tc.Translation,
        Vec3(e.GetComponent<SphereColliderComponent>().Radius * 2.0f),
        Vec4(0, 1, 0, 1),
        tc.Rotation);
    }
  }
}
