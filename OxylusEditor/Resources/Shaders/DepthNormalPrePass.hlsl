#define DEPTH_PASS
#include "ObjectVS.hlsl"

#include "PBRCommon.hlsli"

float4 PSmain(VertexOutput input) : SV_Target0 {
  Material material = GetMaterial(push_const.material_index + input.draw_index);

  float2 scaledUV = input.uv;
  scaledUV *= material.uv_scale;

  float4 baseColor;
  const SamplerState materialSampler = Samplers[material.sampler];
  if (material.albedo_map_id != INVALID_ID) {
    baseColor = GetMaterialAlbedoTexture(material).Sample(materialSampler, scaledUV) * material.color;
  }
  else {
    baseColor = material.color;
  }

  if (material.alpha_mode == ALPHA_MODE_MASK) {
    clip(baseColor.a - material.alpha_cutoff);
  }

  float3 normal = normalize(input.normal);

  const bool useNormalMap = material.normal_map_id != INVALID_ID;
  if (useNormalMap) {
    float3 bumpColor = GetMaterialNormalTexture(material).Sample(materialSampler, scaledUV).rgb;
    bumpColor = bumpColor * 2.f - 1.f;

    const float3x3 TBN = GetNormalTangent(input.world_pos, input.normal, scaledUV);
    normal = normalize(lerp(normal, mul(bumpColor, TBN), length(bumpColor)));
  }

  return float4(normal, 1.0f);
}
