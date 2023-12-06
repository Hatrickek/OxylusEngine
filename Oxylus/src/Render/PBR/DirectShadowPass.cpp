#include "DirectShadowPass.h"

#include <vuk/Partials.hpp>

#include "Core/Application.h"
#include "Core/Entity.h"

#include "Render/RendererConfig.h"

namespace Oxylus {
void DirectShadowPass::update_cascades(const Vec3& dir_light_transform, Camera* camera, DirectShadowUB* cascades_ubo) {
  OX_SCOPED_ZONE;
  float cascade_splits[SHADOW_MAP_CASCADE_COUNT];

  float near_clip = camera->get_near();
  float far_clip = camera->get_far();
  float clip_range = far_clip - near_clip;

  float min_z = near_clip;
  float max_z = near_clip + clip_range;

  float range = max_z - min_z;
  float ratio = max_z / min_z;

  constexpr float cascade_split_lambda = 0.95f;

  // Calculate split depths based on view camera frustum
  // Based on method presented in https://developer.nvidia.com/gpugems/GPUGems3/gpugems3_ch10.html
  for (uint32_t i = 0; i < SHADOW_MAP_CASCADE_COUNT; i++) {
    float p = (i + 1) / static_cast<float>(SHADOW_MAP_CASCADE_COUNT);
    float log = min_z * std::pow(ratio, p);
    float uniform = min_z + range * p;
    float d = cascade_split_lambda * (log - uniform) + uniform;
    cascade_splits[i] = (d - near_clip) / clip_range;
  }

  // Calculate orthographic projection matrix for each cascade
  float last_split_dist = 0.0;
  for (uint32_t i = 0; i < SHADOW_MAP_CASCADE_COUNT; i++) {
    float split_dist = cascade_splits[i];

    Vec3 frustum_corners[8] = {
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
    Mat4 inv_cam = inverse(camera->get_projection_matrix() * camera->get_view_matrix());
    for (uint32_t j = 0; j < 8; j++) {
      Vec4 inv_corner = inv_cam * Vec4(frustum_corners[j], 1.0f);
      frustum_corners[j] = inv_corner / inv_corner.w;
    }

    for (uint32_t j = 0; j < 4; j++) {
      Vec3 dist = frustum_corners[j + 4] - frustum_corners[j];
      frustum_corners[j + 4] = frustum_corners[j] + (dist * split_dist);
      frustum_corners[j] = frustum_corners[j] + (dist * last_split_dist);
    }

    // Get frustum center
    Vec3 frustum_center = Vec3(0.0f);
    for (uint32_t j = 0; j < 8; j++) {
      frustum_center += frustum_corners[j];
    }
    frustum_center /= 8.0f;

    float radius = 0.0f;
    for (uint32_t j = 0; j < 8; j++) {
      float distance = glm::length(frustum_corners[j] - frustum_center);
      radius = glm::max(radius, distance);
    }
    radius = std::ceil(radius * 16.0f) / 16.0f;

    Vec3 max_extents = Vec3(radius);
    Vec3 min_extents = -max_extents;

    auto direction = -dir_light_transform;

    Mat4 light_view_matrix = lookAt(frustum_center - direction * -min_extents.z, frustum_center, Vec3(0.0f, 0.0f, 1.0f));
    Mat4 light_ortho_matrix = glm::ortho(min_extents.x, max_extents.x, min_extents.y, max_extents.y, 0.0f, max_extents.z - min_extents.z);

    cascades_ubo->cascade_splits[i] = (camera->get_near() + split_dist * clip_range) * -1.0f;
    cascades_ubo->cascade_view_proj_mat[i] = light_ortho_matrix * light_view_matrix;

    last_split_dist = cascade_splits[i];

    cascades_ubo->scissor_normalized = get_clamp_to_edge_coords();
  }
}

Vec4 DirectShadowPass::get_clamp_to_edge_coords() {
  constexpr float border = 0.5f;

  float const texel = 1.0f / float(4 * RendererCVar::cvar_shadows_size.get());
  auto const dim = float(RendererCVar::cvar_shadows_size.get());
  float const l = border;
  float const b = border;
  float const w = dim - 2.0f * border;
  float const h = dim - 2.0f * border;
  return Vec4{l, b, l + w, b + h} * texel;
}
}
