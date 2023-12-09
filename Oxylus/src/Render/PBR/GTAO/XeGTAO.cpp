#include "XeGTAO.h"

#include "Render/Camera.h"
#include <glm/gtc/type_ptr.hpp>

namespace XeGTAO {
void gtao_update_constants(GTAOConstants& consts, const int viewport_width, const int viewport_height, const GTAOSettings& settings, const Oxylus::Camera* camera, const unsigned frameCounter) {
  consts.ViewportSize = {viewport_width, viewport_height};
  consts.ViewportPixelSize = {1.0f / static_cast<float>(viewport_width), 1.0f / static_cast<float>(viewport_height)};

  const float depth_linearize_mul = camera->get_far() * camera->get_near() / (camera->get_far() - camera->get_near());
  float depth_linearize_add = camera->get_far() / (camera->get_far() - camera->get_near());

  auto proj_mat = camera->get_projection_matrix();
  const auto proj_mat_ptr = value_ptr(proj_mat);

  // correct the handedness issue. need to make sure this below is correct, but I think it is.
  if (depth_linearize_mul * depth_linearize_add < 0)
    depth_linearize_add = -depth_linearize_add;
  consts.DepthUnpackConsts = {depth_linearize_mul, depth_linearize_add};

  const float tan_half_fovy = 1.0f / proj_mat_ptr[1 + 1 *4];
  const float tan_half_fovx = 1.0F /  proj_mat_ptr[0 + 0 *4];
  consts.CameraTanHalfFOV = {tan_half_fovx, tan_half_fovy};

  consts.NDCToViewMul = {consts.CameraTanHalfFOV.x * 2.0f, consts.CameraTanHalfFOV.y * -2.0f};
  consts.NDCToViewAdd = {consts.CameraTanHalfFOV.x * -1.0f, consts.CameraTanHalfFOV.y * 1.0f};

  consts.NDCToViewMul_x_PixelSize = {
    consts.NDCToViewMul.x * consts.ViewportPixelSize.x,
    consts.NDCToViewMul.y * consts.ViewportPixelSize.y
  };

  consts.EffectRadius = settings.Radius;

  consts.EffectFalloffRange = settings.FalloffRange;
  consts.DenoiseBlurBeta = (settings.DenoisePasses == 0)
                             ? (1e4f)
                             : (1.2f);    // high value disables denoise - more elegant & correct way would be do set all edges to 0

  consts.RadiusMultiplier = settings.RadiusMultiplier;
  consts.SampleDistributionPower = settings.SampleDistributionPower;
  consts.ThinOccluderCompensation = settings.ThinOccluderCompensation;
  consts.FinalValuePower = settings.FinalValuePower;
  consts.DepthMIPSamplingOffset = settings.DepthMIPSamplingOffset;
  consts.NoiseIndex = settings.DenoisePasses > 0 ? frameCounter % 64u : 0u;
  consts.Padding0 = 0;
}
}
