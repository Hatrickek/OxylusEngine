#version 450
#pragma shader_stage(fragment)

struct Parameters {
  int Tonemapper;			// 0- Aces 1- Uncharted 2-Filmic 3- Reinhard
  float Exposure;
  float Gamma;
  bool EnableBloom;
  bool EnableSSR;
  vec4 VignetteColor;        // rgb: color, a: intensity
  vec4 VignetteOffset;       // xy: offset, z: useMask, w: enable/disable effect
  vec2 FilmGrain;            // x: enable, y: amount
  vec2 ChromaticAberration;  // x: enable, y: amount
  vec2 Sharpen;              // x: enable, y: amount
  vec2 _pad;
};

#include "PostProcess/PostProcessing.glsl"
#include "PostProcess/Tonemaps.glsl"

layout(binding = 0) uniform sampler2D in_Color;
layout(binding = 4) uniform sampler2D in_Bloom;

layout(binding = 5) uniform UBO_Parameters { Parameters u_Parameters; };

layout(location = 0) out vec4 out_Color;

layout(location = 0) in vec2 in_UV;

void main() {
    vec4 finalImage = texture(in_Color, in_UV).rgba;
    
    vec4 bloom = texture(in_Bloom, in_UV);

    if (u_Parameters.EnableBloom) {
      finalImage += bloom;
    }

    vec3 finalColor = finalImage.rgb * u_Parameters.Exposure;

    // Tonemapping
    finalColor = ApplyTonemap(finalColor, u_Parameters.Tonemapper);

    // Vignette
    if (u_Parameters.VignetteOffset.w > 0.0) {
      finalColor *= PP_Vignette(u_Parameters.VignetteOffset, u_Parameters.VignetteColor, in_UV);
    }

    // Film Grain
    if (u_Parameters.FilmGrain.x > 0) {
      finalColor += PP_FilmGrain(in_UV, u_Parameters.FilmGrain.y);
    }

    // Chromatic Aberration
    if (u_Parameters.ChromaticAberration.x > 0) {
      vec3 ca = PP_ChromaticAberration(in_Color, in_UV, u_Parameters.ChromaticAberration.y);
      finalColor += ca;
    }

    // Sharpen
    if (u_Parameters.Sharpen.x > 0) {
      vec3 sharpen = PP_Sharpen(in_Color, u_Parameters.Sharpen.y);
      finalColor += sharpen;
    }

    // Gamma correction
    finalColor = pow(finalColor, vec3(1.0f / u_Parameters.Gamma));

    out_Color = vec4(finalColor, 1.0);
}