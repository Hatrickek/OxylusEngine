#include "DirectShadowPass.h"

#include <vuk/Partials.hpp>

#include "Core/Entity.h"

namespace Oxylus {

void DirectShadowPass::UpdateCascades(const Entity& dirLightEntity, Camera* camera, DirectShadowUB& cascadesUbo) {
  OX_SCOPED_ZONE;
  float cascadeSplits[SHADOW_MAP_CASCADE_COUNT]{};

  float nearClip = camera->NearClip;
  float farClip = camera->FarClip;
  float clipRange = farClip - nearClip;

  float minZ = nearClip;
  float maxZ = nearClip + clipRange;

  float range = maxZ - minZ;
  float ratio = maxZ / minZ;

  constexpr float cascadeSplitLambda = 0.95f;
  // Calculate split depths based on view camera frustum
  // Based on method presented in https://developer.nvidia.com/gpugems/GPUGems3/gpugems3_ch10.html
  for (uint32_t i = 0; i < SHADOW_MAP_CASCADE_COUNT; i++) {
    float p = (float)(i + 1) / static_cast<float>(SHADOW_MAP_CASCADE_COUNT);
    float log = minZ * std::pow(ratio, p);
    float uniform = minZ + range * p;
    float d = cascadeSplitLambda * (log - uniform) + uniform;
    cascadeSplits[i] = (d - nearClip) / clipRange;
  }

  const Mat4 invCam = inverse(camera->GetProjectionMatrix() * camera->GetViewMatrix());

  float lastSplitDist = 0.0f;

  // Calculate orthographic projection matrix for each cascade
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
    for (uint32_t j = 0; j < 8; j++) {
      Vec4 invCorner = invCam * Vec4(frustumCorners[j], 1.0f);
      frustumCorners[j] = invCorner / invCorner.w;
    }

    for (uint32_t j = 0; j < 4; j++) {
      Vec3 dist = frustumCorners[j + 4] - frustumCorners[j];
      frustumCorners[j + 4] = frustumCorners[j] + dist * splitDist;
      frustumCorners[j] = frustumCorners[j] + dist * lastSplitDist;
    }

    lastSplitDist = cascadeSplits[i];

    // Get frustum center
    auto frustumCenter = Vec3(0.0f);
    for (uint32_t j = 0; j < 8; j++) {
      frustumCenter += frustumCorners[j];
    }
    frustumCenter /= 8.0f;

    //float radius = 0.0f;
    //for (uint32_t j = 0; j < 8; j++) {
    //  float distance = glm::distance(frustumCorners[j], frustumCenter);
    //  radius = glm::max(radius, distance);
    //}

    // Temp workaround to flickering when rotating camera
    // Sphere radius for lightOrthoMatrix should fix this
    // But radius changes as the camera is rotated which causes flickering
    //constexpr float value = 1.0f; // 16.0f;
    //radius = std::ceil(radius * value) / value;

    // Divide the value by 5 and round it up to the nearest integer
    //int roundedValue = static_cast<int>(std::ceil(radius / 5.0f));

    // Multiply the rounded value by 5 to get the nearest multiple of 5
    //float result = (float)roundedValue * 5.0f;

    float radius = 0.0f;
    for (uint32_t j = 0; j < 8; j++) {
      float distance = length(frustumCorners[j] - frustumCenter);
      radius = glm::max(radius, distance);
    }
    radius = std::ceil(radius * 16.0f) / 16.0f;

    auto maxExtents = Vec3(radius);
    Vec3 minExtents = -maxExtents;

    auto& lc = dirLightEntity.GetComponent<LightComponent>();

    Vec3 lightDir = normalize(-lc.Direction);
    float cascadeFarPlaneOffset = 50.0f, cascadeNearPlaneOffset = -50.0f;
    Mat4 lightOrthoMatrix = glm::ortho(minExtents.x,
      maxExtents.x,
      minExtents.y,
      maxExtents.y,
      cascadeNearPlaneOffset,
      maxExtents.z - minExtents.z + cascadeFarPlaneOffset);
    Mat4 lightViewMatrix = lookAt(frustumCenter - lightDir * -minExtents.z, frustumCenter, Vec3(0.0f, 1.0f, 0.0f));
    auto shadowProj = lightOrthoMatrix * lightViewMatrix;

    constexpr bool stabilizeCascades = true;
    if (stabilizeCascades) {
      // Create the rounding matrix, by projecting the world-space origin and determining
      // the fractional offset in texel space
      glm::vec4 shadowOrigin = glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
      shadowOrigin = shadowProj * shadowOrigin;
      shadowOrigin *= (float)RendererConfig::Get()->DirectShadowsConfig.Size * 0.5f;

      glm::vec4 roundedOrigin = round(shadowOrigin);
      glm::vec4 roundOffset = roundedOrigin - shadowOrigin;
      roundOffset = roundOffset * (2.0f / (float)RendererConfig::Get()->DirectShadowsConfig.Size);
      roundOffset.z = 0.0f;
      roundOffset.w = 0.0f;

      lightOrthoMatrix[3] += roundOffset;
    }

    // Store split distance and matrix in cascade
    cascadesUbo.cascadeSplits[i] = (camera->NearClip + splitDist * clipRange) * -1.0f;
    cascadesUbo.cascadeViewProjMat[i] = lightOrthoMatrix * lightViewMatrix;
  }
}
}
