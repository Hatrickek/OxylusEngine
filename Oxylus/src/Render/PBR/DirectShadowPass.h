#pragma once
#include "Core/Types.h"

namespace ox {
class VulkanDescriptorSet;
class Camera;

constexpr auto SHADOW_MAP_CASCADE_COUNT = 4;

class DirectShadowPass {
public:
  struct DirectShadowUB {
    Mat4 cascade_view_proj_mat[SHADOW_MAP_CASCADE_COUNT]{};
    Vec4 cascade_splits = {};
    Vec4 scissor_normalized = {};
  };

  static void update_cascades(const Vec3& dir_light_transform, Camera* camera, DirectShadowUB* cascades_ubo);

  static Vec4 get_clamp_to_edge_coords();
};
}
