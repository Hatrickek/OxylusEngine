#include "XeGTAO.h"

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

void gtao_update_constants(GTAOConstants& consts, int viewportWidth, int viewportHeight, const GTAOSettings& settings, const float projMatrix[16], bool rowMajor, unsigned frameCounter) {
  consts.ViewportSize = {viewportWidth, viewportHeight};
  consts.ViewportPixelSize = {1.0f / static_cast<float>(viewportWidth), 1.0f / static_cast<float>(viewportHeight)};

  float depthLinearizeMul = (rowMajor)
                              ? (-projMatrix[3 * 4 + 2])
                              : (-projMatrix[3 + 2 *
                                             4]);     // float depthLinearizeMul = ( clipFar * clipNear ) / ( clipFar - clipNear );
  float depthLinearizeAdd = (rowMajor)
                              ? (projMatrix[2 * 4 + 2])
                              : (projMatrix[2 + 2 *
                                            4]);     // float depthLinearizeAdd = clipFar / ( clipFar - clipNear );

  // correct the handedness issue. need to make sure this below is correct, but I think it is.
  if (depthLinearizeMul * depthLinearizeAdd < 0)
    depthLinearizeAdd = -depthLinearizeAdd;
  consts.DepthUnpackConsts = {depthLinearizeMul, depthLinearizeAdd};

  float tanHalfFOVY = 1.0f / ((rowMajor)
                                ? (projMatrix[1 * 4 + 1])
                                : (projMatrix[1 + 1 *
                                              4]));    // = tanf( drawContext.Camera.GetYFOV( ) * 0.5f );
  float tanHalfFOVX = 1.0F / ((rowMajor)
                                ? (projMatrix[0 * 4 + 0])
                                : (projMatrix[0 + 0 *
                                              4]));    // = tanHalfFOVY * drawContext.Camera.GetAspect( );
  consts.CameraTanHalfFOV = {tanHalfFOVX, tanHalfFOVY};

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
  consts.NoiseIndex = (settings.DenoisePasses > 0) ? (frameCounter % 64) : (0);
  consts.Padding0 = 0;
}
}
