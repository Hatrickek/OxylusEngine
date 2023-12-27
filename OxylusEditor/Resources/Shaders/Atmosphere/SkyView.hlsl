#define HAS_TRANSMITTANCE_LUT
#define HAS_MULTISCATTER_LUT
#include "SkyCommon.hlsli"

Texture2D<float4> TransmittanceLUT : register(t0);
Texture2D<float4> MultiScatteringLUT : register(t1);
SamplerState LinearSampler : register(s2);

cbuffer ViewBuffer : register(b3) {
  float3 CameraPosition;
  float _pad;
  float3 SunDirection;
  float _pad2;
  float3 SunColor;
  float _pad3;
}

struct VSInput {
  float4 Position : SV_Position;
  float2 UV : TEXCOORD;
};

float4 main(VSInput input) : SV_TARGET {
  AtmosphereParameters atmosphere;
  InitAtmosphereParameters(atmosphere);

  float3 skyRelativePosition = CameraPosition;
  float3 worldPosition = GetCameraPlanetPos(atmosphere, skyRelativePosition);

  float viewHeight = length(worldPosition);

  float viewZenithCosAngle;
  float lightViewCosAngle;
  UvToSkyViewLutParams(atmosphere, viewZenithCosAngle, lightViewCosAngle, viewHeight, input.UV);

  float3 sunDirection;
  {
    float3 upVector = min(worldPosition / viewHeight, 1.0); // Causes flickering without min(x, 1.0) for untouched/edited directional lights
    float sunZenithCosAngle = dot(upVector, SunDirection);
    sunDirection = normalize(float3(sqrt(1.0 - sunZenithCosAngle * sunZenithCosAngle), 0.0, sunZenithCosAngle));
  }

  worldPosition = float3(0.0, 0.0, viewHeight);

  float viewZenithSinAngle = sqrt(1 - viewZenithCosAngle * viewZenithCosAngle);
  float3 worldDirection = float3(
    viewZenithSinAngle * lightViewCosAngle,
    viewZenithSinAngle * sqrt(1.0 - lightViewCosAngle * lightViewCosAngle),
    viewZenithCosAngle);

  // Move to top atmosphere
  if (!MoveToTopAtmosphere(worldPosition, worldDirection, atmosphere.topRadius)) {
    // Ray is not intersecting the atmosphere
    return float4(0, 0, 0, 1);
  }

  float3 sunIlluminance = SunColor;

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

  SingleScatteringResult ss = IntegrateScatteredLuminance(
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
    TransmittanceLUT,
    LinearSampler,
    MultiScatteringLUT);

  float3 L = ss.L;

  return float4(L, 1.0);
}
