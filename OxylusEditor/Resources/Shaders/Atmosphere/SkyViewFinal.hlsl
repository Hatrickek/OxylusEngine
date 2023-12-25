#define HAS_TRANSMITTANCE_LUT
#define HAS_MULTISCATTER_LUT
#include "SkyCommon.hlsli"

cbuffer ViewBuffer : register(b0) {
  float3 CameraPosition;
  float _pad;
  float3 SunDirection;
  float _pad2;
  float3 SunColor;
  float _pad3;
}

cbuffer SkyFrustumBuffer : register(b1) {
  float4x4 InvVP;
}

Texture2D<float4> TransmittanceLUT : register(t2);
Texture2D<float4> MultiScatteringLUT : register(t3);
SamplerState LinearSampler : register(s4);

float3 AccurateAtmosphericScattering(float2 pixelPosition,
                                     float3 rayOrigin,
                                     float3 rayDirection,
                                     float3 sunDirection,
                                     float3 sunColor,
                                     bool enableSun,
                                     bool darkMode,
                                     bool perPixelNoise,
                                     bool receiveShadow) {
  AtmosphereParameters atmosphere;
  InitAtmosphereParameters(atmosphere);

  float3 worldPosition = GetCameraPlanetPos(atmosphere, rayOrigin);
  const float3 worldDirection = rayDirection;

  float3 luminance = 0;
  // Move to top atmosphere as the starting point for ray marching.
  // This is critical to be after the above to not disrupt above atmosphere tests and voxel selection.
  if (MoveToTopAtmosphere(worldPosition, worldDirection, atmosphere.topRadius)) {
    // Apply the start offset after moving to the top of atmosphere to avoid black pixels
    worldPosition += worldDirection * AP_START_OFFSET_KM;

    const float3 sunIlluminance = sunColor;

    const float tDepth = 0.0;
    const float sampleCountIni = 0.0;
    const bool variableSampleCount = true;
    const bool opaque = false;
    const bool ground = false;
    const bool mieRayPhase = true;
    const bool multiScatteringApprox = true;
    const bool volumetricCloudShadow = receiveShadow;
    const bool opaqueShadow = receiveShadow;
    const SingleScatteringResult ss = IntegrateScatteredLuminance(
      atmosphere,
      pixelPosition,
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
      MultiScatteringLUT
    );

    luminance = ss.L;
  }

  float3 totalColor;

  if (enableSun) {
    const float3 sunIlluminance = sunColor;
    totalColor = luminance + GetSunLuminance(worldPosition, worldDirection, sunDirection, sunIlluminance, atmosphere, TransmittanceLUT, LinearSampler);
  }
  else {
    totalColor = luminance; // We cant really seperate mie from luminance due to precomputation, todo?
  }

  if (darkMode) {
    totalColor = max(pow(saturate(dot(sunDirection, rayDirection)), 64) * sunColor, 0) * luminance * 1.0;
  }

#if HEIGHT_FOG // TODO
  if (GetFrame().options & OPTION_BIT_HEIGHT_FOG) {
    const float3 planet_center = atmosphere.planetCenter * SKY_UNIT_TO_M;
    const float bottom_radius = atmosphere.bottomRadius * SKY_UNIT_TO_M;
    const float top_radius = bottom_radius + GetWeather().fog.height_end;
    float dist = RaySphereIntersectNearest(rayOrigin, rayDirection, planet_center, top_radius);

    if (dist >= 0) {
      // Offset origin with fog start value.
      // We can't do this with normal distance due to infinite distance.
      const float3 offsetO = rayOrigin + rayDirection * GetWeather().fog.start;
      float4 fog = GetFog(dist, offsetO, rayDirection);
      if (fog.a > 0) // this check avoids switching to fully fogged above fog level camera at zero density
      {
        if (length(rayOrigin - planet_center) > top_radius) // check if we are above fog height sphere
        {
          // hack: flip fog when camera is above
          fog.a = 1 - fog.a; // this only supports non-premultiplied fog
        }
        totalColor = lerp(totalColor, fog.rgb, fog.a); // non-premultiplied fog
      }
    }
  }
#endif

  return totalColor;
}

float3 GetDynamicSkyColor(in float2 pixel, in float3 V, bool sunEnabled = true, bool darkEnabled = false, bool stationary = false, bool perPixelNoise = false, bool receiveShadow = false) {
  float3 sky = AccurateAtmosphericScattering(
    pixel,
    CameraPosition,
    // Ray origin
    V,
    // Ray direction
    SunDirection,
    // Position of the sun
    SunColor,
    // Sun Color
    sunEnabled,
    // Use sun and total
    darkEnabled,
    // Skip color lookup from lut
    perPixelNoise,
    // Vary sampling position with TAA
    receiveShadow // Atmosphere to use pre-rendered shadow data
  );

  sky *= 3.0f; //GetWeather().sky_exposure;

  return sky;
}

float4 main(float4 pos : SV_POSITION, float2 clipspace : TEXCOORD) : SV_TARGET {
  const float4 ndc[] = {
    float4(-1.0f, -1.0f, 1.0f, 1.0f), // bottom-left
    float4(1.0f, -1.0f, 1.0f, 1.0f),  // bottom-right
    float4(-1.0f, 1.0f, 1.0f, 1.0f),  // top-left
    float4(1.0f, 1.0f, 1.0f, 1.0f)    // top-right
  };

  float4 inv_corner = mul(InvVP, ndc[0]);
  const float4 frustumX = inv_corner / inv_corner.w;

  inv_corner = mul(InvVP, ndc[1]);
  const float4 frustumY = inv_corner / inv_corner.w;

  inv_corner = mul(InvVP, ndc[2]);
  const float4 frustumZ = inv_corner / inv_corner.w;

  inv_corner = mul(InvVP, ndc[3]);
  const float4 frustumW = inv_corner / inv_corner.w;

  const float3 direction = normalize(
    lerp(lerp(frustumX, frustumY, clipspace.x),
         lerp(frustumZ, frustumW, clipspace.x),
         clipspace.y)
  );

  float4 color = float4(GetDynamicSkyColor(clipspace.xy, direction), 1);

  color = clamp(color, 0, 65000);
  return color;
}
