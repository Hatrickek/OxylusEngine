#define HAS_TRANSMITTANCE_LUT
#define HAS_MULTISCATTER_LUT
#include "SkyCommon.hlsli"

struct VSInput {
  float4 Position : SV_Position;
  float2 UV : TEXCOORD;
};

float4 main(VSInput input) : SV_TARGET {
  AtmosphereParameters atmosphere;
  InitAtmosphereParameters(atmosphere);

  const float3 skyRelativePosition = GetCamera().Position;
  float3 worldPosition = GetCameraPlanetPos(atmosphere, skyRelativePosition);

  float viewHeight = length(worldPosition);

  float viewZenithCosAngle;
  float lightViewCosAngle;
  UvToSkyViewLutParams(atmosphere, viewZenithCosAngle, lightViewCosAngle, viewHeight, input.UV);

  const float3 upVector = min(worldPosition / viewHeight, 1.0); // Causes flickering without min(x, 1.0) for untouched/edited directional lights
  float sunZenithCosAngle = dot(upVector, GetScene().SunDirection);
  const float3 sunDirection = normalize(float3(sqrt(1.0 - sunZenithCosAngle * sunZenithCosAngle), 0.0, sunZenithCosAngle));

  worldPosition = float3(0.0, 0.0, viewHeight);

  const float viewZenithSinAngle = sqrt(1 - viewZenithCosAngle * viewZenithCosAngle);
  const float3 worldDirection = float3(
    viewZenithSinAngle * lightViewCosAngle,
    viewZenithSinAngle * sqrt(1.0 - lightViewCosAngle * lightViewCosAngle),
    viewZenithCosAngle);

  // Move to top atmosphere
  if (!MoveToTopAtmosphere(worldPosition, worldDirection, atmosphere.topRadius)) {
    // Ray is not intersecting the atmosphere
    return float4(0, 0, 0, 1);
  }

  const float3 sunIlluminance = GetScene().SunColor;

  const float tDepth = 0.0;
  const float sampleCountIni = 30.0;
  const bool variableSampleCount = true;
  const bool perPixelNoise = false;
  const bool opaque = false;
  const bool ground = false;
  const bool mieRayPhase = true;
  const bool multiScatteringApprox = true;
  const bool volumetricCloudShadow = false;
  const bool opaqueShadow = false;

  const SingleScatteringResult ss = IntegrateScatteredLuminance(
    atmosphere,
    float2(0.0f, 0.0f),
    // not used
    worldPosition,
    worldDirection,
    sunDirection,
    sunIlluminance,
    tDepth,
    sampleCountIni,
    variableSampleCount,
    perPixelNoise,
    opaque,
    ground,
    mieRayPhase,
    multiScatteringApprox,
    volumetricCloudShadow,
    opaqueShadow,
    GetSkyTransmittanceLUTTexture(),
    GetSkyMultiScatterLUTTexture());

  float3 L = ss.L;

  return float4(L, 1.0);
}
