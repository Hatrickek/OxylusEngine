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
    Mat4 cascadeViewProjMat[SHADOW_MAP_CASCADE_COUNT]{};
    float cascadeSplits[4]{};
  };

  static void UpdateCascades(const Entity& dirLightEntity, Camera* camera, DirectShadowUB& cascadesUbo);
};
}
