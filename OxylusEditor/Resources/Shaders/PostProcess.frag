#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) out vec4 out_Color;

layout(location = 1) in vec2 in_TexCoords;

layout(binding = 0) uniform sampler2D in_Color;

layout(binding = 1) uniform UBOParams {
  int tonemapper; // 0- Aces 1- Uncharted 2-Filmic 3- Reinhard
  float exposure;
  float gamma;
  bool enableSSAO;
  bool enableBloom;
  bool enableSSR;
  vec2 _pad;
  vec4 vignetteColor;  // rgb: color, a: intensity
  vec4 vignetteOffset; // xy: offset, z: useMask, w: enable/disable effect
}
u_UBOParams;

#include "PostProcessing.glsl"
#include "Tonemaps.glsl"

void main() {
  vec4 colorPass = texture(in_Color, in_TexCoords);

  vec3 finalColor = colorPass.rgb * u_UBOParams.exposure;

  // Tone mapping
  switch (u_UBOParams.tonemapper) {
  case 0:
    finalColor = TonemapAces(finalColor);
    break;
  case 1:
    finalColor = TonemapUncharted2(finalColor);
    break;
  case 2:
    finalColor = TonemapFilmic(finalColor);
    break;
  case 3:
    finalColor = TonemapReinhard(finalColor);
    break;
  default:
    finalColor = TonemapAces(finalColor);
    break;
  }

  // Apply vignette
  if (u_UBOParams.vignetteOffset.w > 0.0) {
    finalColor *= ApplyVignette(u_UBOParams.vignetteOffset,
                                u_UBOParams.vignetteColor, in_TexCoords);
  }

  finalColor = pow(finalColor, vec3(1.0f / u_UBOParams.gamma));

  out_Color = vec4(finalColor, 1.0);
}