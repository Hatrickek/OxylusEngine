#define DEPTH_PASS
#include "ObjectVS.hlsl"

#include "PBRCommon.hlsli"

struct PixelOutput {
  float4 normals;
  float2 velocity;
};

PixelOutput PSmain(VertexOutput input) : SV_Target0 {
  Material material = GetMaterial(push_const.material_index + input.draw_index);

  float2 scaledUV = input.uv;
  scaledUV *= material.uv_scale;

  float4 baseColor;
  const SamplerState materialSampler = Samplers[material.sampler];
  if (material.albedo_map_id != INVALID_ID) {
    baseColor = GetMaterialAlbedoTexture(material).Sample(materialSampler, scaledUV) * material.color;
  } else {
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

  const float2 clipspace = uv_to_clipspace(input.uv);

  float2 velocity = 0;
  float2 pos2D = clipspace;
  float4 pos2DPrev = input.prev_position;
  if (pos2DPrev.w > 0) {
    pos2DPrev.xy /= pos2DPrev.w;
    const float2 vel = ((pos2DPrev.xy - get_camera().temporalaa_jitter_prev) - (pos2D.xy - get_camera().temporalaa_jitter)) * float2(0.5, -0.5);
    velocity = clamp(vel, -1, 1);
  }

  PixelOutput output;
  output.normals = float4(normal, 1.0f);
  output.velocity = velocity;

  return output;
}
