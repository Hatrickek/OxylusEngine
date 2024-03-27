#include "Globals.hlsli"

struct VSInput {
  float4 position : SV_Position;
  float2 uv : TEXCOORD;
};

float3 vignette(float4 vignetteOffset, float4 vignetteColor, float2 texCoords) {
  float3 final_color = float3(1, 1, 1);
  if (vignetteOffset.z > 0.0) {
    const float vignette_opacity = vignetteColor.a;
    final_color = lerp(final_color, vignetteColor.rgb, vignette_opacity);
  } else {
    float2 uv = texCoords + vignetteOffset.xy;
    uv *= 1.0 - (texCoords.yx + vignetteOffset.yx);
    float vig = uv.x * uv.y * 15.0;
    vig = pow(vig, vignetteColor.a);
    vig = clamp(vig, 0.0, 1.0);
    final_color = lerp(vignetteColor.rgb, final_color, vig);
  }
  return final_color;
}

float3 tonemap_aces(const float3 x) {
  const float a = 2.51;
  const float b = 0.03;
  const float c = 2.43;
  const float d = 0.59;
  const float e = 0.14;
  return clamp((x * (a * x + b)) / (x * (c * x + d) + e), float3(0, 0, 0), float3(0, 0, 0));
}

float4 main(VSInput input) : SV_TARGET {
  float4 final_image = GetForwardTexture().Sample(LINEAR_CLAMPED_SAMPLER, input.uv);

  const float4 bloom = GetBloomTexture().Sample(LINEAR_CLAMPED_SAMPLER, input.uv);

  if (get_scene().post_processing_data.enable_bloom) {
    final_image += bloom;
  }

  float3 final_color = final_image.rgb * get_scene().post_processing_data.exposure;

  // Tonemapping
  final_color = tonemap_aces(final_color);

  // Vignette
  if (get_scene().post_processing_data.vignette_offset.w > 0.0) {
    final_color *= vignette(get_scene().post_processing_data.vignette_offset, get_scene().post_processing_data.vignette_color, input.uv);
  }

  // Gamma correction
  final_color = pow(final_color, float3((1.0f / get_scene().post_processing_data.gamma).rrr));

  return float4(final_color, 1.0);
}
