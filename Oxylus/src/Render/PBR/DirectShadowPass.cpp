#include "DirectShadowPass.h"

#include <vuk/Partials.hpp>

#include "Core/Application.h"
#include "Core/Entity.h"

namespace Oxylus {
float round_up_to_nearest_multiple_of5(float value) {
  // Divide the value by 5 and round it up to the nearest integer
  const float roundedValue = static_cast<float>(std::ceil(value / 5.0));

  // Multiply the rounded value by 5 to get the nearest multiple of 5
  const float result = roundedValue * 5.0f;

  return result;
}

void DirectShadowPass::update_cascades(const Vec3& dir_light_transform, Camera* camera, DirectShadowUB* cascades_ubo) {
  OX_SCOPED_ZONE;
  float cascadeSplits[SHADOW_MAP_CASCADE_COUNT];

  float nearClip = camera->near_clip;
  float farClip = camera->far_clip;
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

    glm::vec3 frustumCorners[8] = {
      glm::vec3(-1.0f, 1.0f, 0.0f),
      glm::vec3(1.0f, 1.0f, 0.0f),
      glm::vec3(1.0f, -1.0f, 0.0f),
      glm::vec3(-1.0f, -1.0f, 0.0f),
      glm::vec3(-1.0f, 1.0f, 1.0f),
      glm::vec3(1.0f, 1.0f, 1.0f),
      glm::vec3(1.0f, -1.0f, 1.0f),
      glm::vec3(-1.0f, -1.0f, 1.0f),
    };

    // Project frustum corners into world space
    glm::mat4 invCam = glm::inverse(camera->get_projection_matrix_flipped() * camera->get_view_matrix());
    for (uint32_t j = 0; j < 8; j++) {
      glm::vec4 invCorner = invCam * glm::vec4(frustumCorners[j], 1.0f);
      frustumCorners[j] = invCorner / invCorner.w;
    }

    for (uint32_t j = 0; j < 4; j++) {
      glm::vec3 dist = frustumCorners[j + 4] - frustumCorners[j];
      frustumCorners[j + 4] = frustumCorners[j] + (dist * splitDist);
      frustumCorners[j] = frustumCorners[j] + (dist * lastSplitDist);
    }

    // Get frustum center
    glm::vec3 frustumCenter = glm::vec3(0.0f);
    for (uint32_t i = 0; i < 8; i++) {
      frustumCenter += frustumCorners[i];
    }
    frustumCenter /= 8.0f;

    float radius = 0.0f;
    for (uint32_t i = 0; i < 8; i++) {
      float distance = glm::length(frustumCorners[i] - frustumCenter);
      radius = glm::max(radius, distance);
    }
    radius = std::ceil(radius * 16.0f) / 16.0f;

    glm::vec3 maxExtents = glm::vec3(radius);
    glm::vec3 minExtents = -maxExtents;

    float angle = dir_light_transform.y;
    float r = 20.0f;
    Vec3 lightPos = glm::vec3(cos(angle) * r, -r, sin(angle) * r);

    glm::vec3 lightDir = normalize(-lightPos);
    glm::mat4 lightViewMatrix = glm::lookAt(frustumCenter - lightDir * -minExtents.z, frustumCenter, glm::vec3(0.0f, 1.0f, 0.0f));
    glm::mat4 lightOrthoMatrix = glm::ortho(minExtents.x, maxExtents.x, minExtents.y, maxExtents.y, 0.0f, maxExtents.z - minExtents.z);

    // Store split distance and matrix in cascade
    cascades_ubo->cascade_splits[i] = (camera->near_clip + splitDist * clipRange) * -1.0f;
    cascades_ubo->cascade_view_proj_mat[i] = lightOrthoMatrix * lightViewMatrix;

    lastSplitDist = cascadeSplits[i];
  }
}
}
