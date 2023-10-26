#pragma once
#include "Core/Types.h"

namespace Oxylus {
class VulkanDescriptorSet;
class Camera;

constexpr auto SHADOW_MAP_CASCADE_COUNT = 4;

class DirectShadowPass {
public:
  struct DirectShadowUB {
    Mat4 cascade_view_proj_mat[SHADOW_MAP_CASCADE_COUNT]{};
    float cascade_splits[4]{};
  };

  static void update_cascades(const Vec3& dir_light_transform, Camera* camera, DirectShadowUB* cascades_ubo);
};
}
