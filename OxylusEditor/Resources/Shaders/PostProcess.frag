#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) out vec4 out_Color;

layout(location = 1) in vec2 in_TexCoords;

layout(binding = 0) uniform sampler2D in_Color;

#include "Parameters.glsl"

layout(binding = 1) uniform UBO_Parameters { Parameters u_Parameters; };

#include "PostProcessing.glsl"
#include "Tonemaps.glsl"

void main() {
  vec4 colorPass = texture(in_Color, in_TexCoords);

  vec3 finalColor = colorPass.rgb * u_Parameters.Exposure;

  // Tone mapping
  switch (u_Parameters.Tonemapper) {
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
  if (u_Parameters.VignetteOffset.w > 0.0) {
    finalColor *= PP_Vignette(u_Parameters.VignetteOffset, u_Parameters.VignetteColor, in_TexCoords);
  }

  if (u_Parameters.FilmGrain.x > 0) {
    finalColor += PP_FilmGrain(in_TexCoords, u_Parameters.FilmGrain.y);
  }

  if (u_Parameters.ChromaticAberration.x > 0) {
    vec3 ca = PP_ChromaticAberration(in_Color, in_TexCoords, u_Parameters.ChromaticAberration.y);
    finalColor += ca;
  }

  if (u_Parameters.Sharpen.x > 0) {
    vec3 sharpen = PP_Sharpen(in_Color, u_Parameters.Sharpen.y);
    finalColor += sharpen;
  }

  finalColor = pow(finalColor, vec3(1.0f / u_Parameters.Gamma));

  out_Color = vec4(finalColor, 1.0);
}