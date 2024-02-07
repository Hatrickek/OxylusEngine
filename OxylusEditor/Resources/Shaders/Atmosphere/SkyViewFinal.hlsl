#define HAS_MULTISCATTER_LUT
#define HAS_TRANSMITTANCE_LUT
#include "SkyCommonShading.hlsli"

struct VSInput {
  float4 Position : SV_Position;
  float2 UV : TEXCOORD;
};

float4 main(VSInput input) : SV_TARGET {
  const float4 ndc[] = {
    float4(-1.0f, -1.0f, 1.0f, 1.0f), // bottom-left
    float4(1.0f, -1.0f, 1.0f, 1.0f),  // bottom-right
    float4(-1.0f, 1.0f, 1.0f, 1.0f),  // top-left
    float4(1.0f, 1.0f, 1.0f, 1.0f)    // top-right
  };

  const float4x4 invVP = GetCamera().InvProjectionViewMatrix;

  float4 inv_corner = mul(invVP, ndc[0]);
  const float4 frustumX = inv_corner / inv_corner.w;

  inv_corner = mul(invVP, ndc[1]);
  const float4 frustumY = inv_corner / inv_corner.w;

  inv_corner = mul(invVP, ndc[2]);
  const float4 frustumZ = inv_corner / inv_corner.w;

  inv_corner = mul(invVP, ndc[3]);
  const float4 frustumW = inv_corner / inv_corner.w;

  const float3 direction = normalize(
    lerp(lerp(frustumX, frustumY, input.UV.x),
         lerp(frustumZ, frustumW, input.UV.x),
         input.UV.y)
  );

  float4 color = float4(GetDynamicSkyColor(input.UV.xy, direction), 1);

  color = clamp(color, 0, 65000);
  return color;
}
