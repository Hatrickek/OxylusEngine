#include "XeGTAO.h"

#include "Render/Camera.h"
#include <glm/gtc/type_ptr.hpp>

namespace XeGTAO {
uint HilbertIndex(uint posX, uint posY) {
  uint index = 0U;
  for (uint curLevel = XE_HILBERT_WIDTH / 2U; curLevel > 0U; curLevel /= 2U) {
    uint regionX = (posX & curLevel) > 0U;
    uint regionY = (posY & curLevel) > 0U;
    index += curLevel * curLevel * ((3U * regionX) ^ regionY);
    if (regionY == 0U) {
      if (regionX == 1U) {
        posX = static_cast<uint>((XE_HILBERT_WIDTH - 1U)) - posX;
        posY = static_cast<uint>((XE_HILBERT_WIDTH - 1U)) - posY;
      }

      uint temp = posX;
      posX = posY;
      posY = temp;
    }
  }
  return index;
}

void gtao_update_constants(GTAOConstants& consts, int viewportWidth, int viewportHeight, const GTAOSettings& settings, const Oxylus::Camera* camera, unsigned frameCounter) {
  consts.ViewportSize = {viewportWidth, viewportHeight};
  consts.ViewportPixelSize = {1.0f / static_cast<float>(viewportWidth), 1.0f / static_cast<float>(viewportHeight)};

  float depth_linearize_mul = camera->get_far() * camera->get_near() / (camera->get_far() - camera->get_near());
  float depth_linearize_add = camera->get_far() / (camera->get_far() - camera->get_near());

  auto projMatrix = value_ptr(camera->get_projection_matrix_flipped());

  //float depth_linearize_mul = -projMatrix[3 + 2 * 4];     // float depthLinearizeMul = ( clipFar * clipNear ) / ( clipFar - clipNear );
  //float depth_linearize_add = projMatrix[2 + 2 * 4];     // float depthLinearizeAdd = clipFar / ( clipFar - clipNear );

  // correct the handedness issue. need to make sure this below is correct, but I think it is.
  if (depth_linearize_mul * depth_linearize_add < 0)
    depth_linearize_add = -depth_linearize_add;
  consts.DepthUnpackConsts = {depth_linearize_mul, depth_linearize_add};

  //float tan_half_fovy = glm::tan(camera->get_fov()) * 0.5f;
  //float tan_half_fovx = tan_half_fovy * camera->get_aspect();
  float tan_half_fovy = 1.0f / projMatrix[1 + 1 *4];    // = tanf( drawContext.Camera.GetYFOV( ) * 0.5f );
  float tan_half_fovx = 1.0F /  projMatrix[0 + 0 *4];    // = tanHalfFOVY * drawContext.Camera.GetAspect( );
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
