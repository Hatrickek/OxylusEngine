#include "XeGTAO.h"

#include "Render/Camera.h"
#include <glm/gtc/type_ptr.hpp>

namespace XeGTAO {
void gtao_update_constants(GTAOConstants& consts, const int viewport_width, const int viewport_height, const GTAOSettings& settings, const ox::Camera* camera, const unsigned frameCounter) {
  consts.ViewportSize = {viewport_width, viewport_height};
  consts.ViewportPixelSize = {1.0f / (float)viewport_width, 1.0f / (float)viewport_height};

  auto p = camera->get_projection_matrix();
  const auto projMatrix = glm::value_ptr(p);

  constexpr auto rowMajor = true;

  float depthLinearizeMul = (rowMajor) ? (-projMatrix[3 * 4 + 2]) : (-projMatrix[3 + 2 * 4]); // float depthLinearizeMul = ( clipFar * clipNear ) / ( clipFar - clipNear );
  float depthLinearizeAdd = (rowMajor) ? (projMatrix[2 * 4 + 2]) : (projMatrix[2 + 2 * 4]);   // float depthLinearizeAdd = clipFar / ( clipFar - clipNear );

  // correct the handedness issue. need to make sure this below is correct, but I think it is.
  if (depthLinearizeMul * depthLinearizeAdd < 0)
    depthLinearizeAdd = -depthLinearizeAdd;
  consts.DepthUnpackConsts = {depthLinearizeMul, depthLinearizeAdd};

  float tanHalfFOVY = 1.0f / ((rowMajor) ? (projMatrix[1 * 4 + 1]) : (projMatrix[1 + 1 * 4])); // = tanf( drawContext.Camera.GetYFOV( ) * 0.5f );
  float tanHalfFOVX = 1.0F / ((rowMajor) ? (projMatrix[0 * 4 + 0]) : (projMatrix[0 + 0 * 4])); // = tanHalfFOVY * drawContext.Camera.GetAspect( );
  consts.CameraTanHalfFOV = {tanHalfFOVX, tanHalfFOVY};

  consts.NDCToViewMul = {consts.CameraTanHalfFOV.x * 2.0f, consts.CameraTanHalfFOV.y * -2.0f};
  consts.NDCToViewAdd = {consts.CameraTanHalfFOV.x * -1.0f, consts.CameraTanHalfFOV.y * 1.0f};

  consts.NDCToViewMul_x_PixelSize = {consts.NDCToViewMul.x * consts.ViewportPixelSize.x, consts.NDCToViewMul.y * consts.ViewportPixelSize.y};

  consts.EffectRadius = settings.Radius;

  consts.EffectFalloffRange = settings.FalloffRange;
  consts.DenoiseBlurBeta = settings.DenoisePasses == 0
                             ? 1e4f
                             : 1.2f; // high value disables denoise - more elegant & correct way would be do set all edges to 0

  consts.RadiusMultiplier = settings.RadiusMultiplier;
  consts.SampleDistributionPower = settings.SampleDistributionPower;
  consts.ThinOccluderCompensation = settings.ThinOccluderCompensation;
  consts.FinalValuePower = settings.FinalValuePower;
  consts.DepthMIPSamplingOffset = settings.DepthMIPSamplingOffset;
  consts.NoiseIndex = settings.DenoisePasses > 0 ? frameCounter % 64u : 0u;
  consts.Padding0 = 0;
}
}
