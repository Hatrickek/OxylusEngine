#include "DirectShadowPass.h"

#include <vuk/Partials.hpp>

#include "Core/Application.h"
#include "Core/Entity.h"

namespace Oxylus {
void DirectShadowPass::update_cascades(const Vec3& dir_light_transform, Camera* camera, DirectShadowUB* cascades_ubo) {
  OX_SCOPED_ZONE;
  float cascadeSplits[SHADOW_MAP_CASCADE_COUNT];

  float nearClip = camera->get_near();
  float farClip = camera->get_far();
  float clipRange = farClip - nearClip;

  float minZ = nearClip;
  float maxZ = nearClip + clipRange;

  float range = maxZ - minZ;
  float ratio = maxZ / minZ;

  constexpr float cascadeSplitLambda = 0.95f;

  // Calculate split depths based on view camera frustum
  // Based on method presented in https://developer.nvidia.com/gpugems/GPUGems3/gpugems3_ch10.html
  for (uint32_t i = 0; i < SHADOW_MAP_CASCADE_COUNT; i++) {
    float p = (i + 1) / static_cast<float>(SHADOW_MAP_CASCADE_COUNT);
    float log = minZ * std::pow(ratio, p);
    float uniform = minZ + range * p;
    float d = cascadeSplitLambda * (log - uniform) + uniform;
    cascadeSplits[i] = (d - nearClip) / clipRange;
  }

  // Calculate orthographic projection matrix for each cascade
  float lastSplitDist = 0.0;
  for (uint32_t i = 0; i < SHADOW_MAP_CASCADE_COUNT; i++) {
    float splitDist = cascadeSplits[i];

    Vec3 frustumCorners[8] = {
      Vec3(-1.0f, 1.0f, 0.0f),
      Vec3(1.0f, 1.0f, 0.0f),
      Vec3(1.0f, -1.0f, 0.0f),
      Vec3(-1.0f, -1.0f, 0.0f),
      Vec3(-1.0f, 1.0f, 1.0f),
      Vec3(1.0f, 1.0f, 1.0f),
      Vec3(1.0f, -1.0f, 1.0f),
      Vec3(-1.0f, -1.0f, 1.0f),
    };

    // Project frustum corners into world space
    Mat4 invCam = inverse(camera->get_projection_matrix_flipped() * camera->get_view_matrix());
    for (uint32_t j = 0; j < 8; j++) {
      Vec4 invCorner = invCam * Vec4(frustumCorners[j], 1.0f);
      frustumCorners[j] = invCorner / invCorner.w;
    }

    for (uint32_t j = 0; j < 4; j++) {
      Vec3 dist = frustumCorners[j + 4] - frustumCorners[j];
      frustumCorners[j + 4] = frustumCorners[j] + (dist * splitDist);
      frustumCorners[j] = frustumCorners[j] + (dist * lastSplitDist);
    }

    // Get frustum center
    Vec3 frustumCenter = Vec3(0.0f);
    for (uint32_t j = 0; j < 8; j++) {
      frustumCenter += frustumCorners[j];
    }
    frustumCenter /= 8.0f;

    float radius = 0.0f;
    for (uint32_t j = 0; j < 8; j++) {
      float distance = glm::length(frustumCorners[j] - frustumCenter);
      radius = glm::max(radius, distance);
    }
    radius = std::ceil(radius * 16.0f) / 16.0f;

    Vec3 maxExtents = Vec3(radius);
    Vec3 minExtents = -maxExtents;

    auto rotation = normalize(-dir_light_transform);

    Mat4 lightViewMatrix = lookAt(frustumCenter - rotation * -minExtents.z, frustumCenter, Vec3(0.0f, 0.0f, 1.0f));
    Mat4 lightOrthoMatrix = glm::ortho(minExtents.x, maxExtents.x, minExtents.y, maxExtents.y, 0.0f, maxExtents.z - minExtents.z);

    cascades_ubo->cascade_splits[i] = (camera->get_near() + splitDist * clipRange) * -1.0f;
    cascades_ubo->cascade_view_proj_mat[i] = lightOrthoMatrix * lightViewMatrix;

    lastSplitDist = cascadeSplits[i];
  }
}
}
