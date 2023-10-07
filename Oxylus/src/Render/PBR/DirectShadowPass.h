#pragma once
#include "Core/Types.h"
#include "Render/Vulkan/VulkanRenderer.h"

namespace Oxylus {
class VulkanDescriptorSet;
class Entity;
class Camera;

constexpr auto SHADOW_MAP_CASCADE_COUNT = 4;

class DirectShadowPass {
public:
  struct DirectShadowUB {
    Mat4 cascade_view_proj_mat[SHADOW_MAP_CASCADE_COUNT]{};
    float cascade_splits[4]{};
  };

  static void update_cascades(const Entity& dir_light_entity, Camera* camera, DirectShadowUB* cascades_ubo);
};
}
