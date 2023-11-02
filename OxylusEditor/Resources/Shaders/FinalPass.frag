#version 450
#pragma shader_stage(fragment)

struct Parameters {
  int Tonemapper;			// 0- Aces 1- Uncharted 2-Filmic 3- Reinhard
  float Exposure;
  float Gamma;
  bool EnableSSAO;
  bool EnableBloom;
  bool EnableSSR;
  vec2 _pad;
  vec4 VignetteColor;        // rgb: color, a: intensity
  vec4 VignetteOffset;       // xy: offset, z: useMask, w: enable/disable effect
  vec2 FilmGrain;            // x: enable, y: amount
  vec2 ChromaticAberration;  // x: enable, y: amount
  vec2 Sharpen;              // x: enable, y: amount
};

#include "PostProcessing.glsl"
#include "Tonemaps.glsl"

layout(binding = 0) uniform sampler2D in_Color;
layout(binding = 3) uniform sampler2D in_SSR;
layout(binding = 4) uniform usampler2D in_SSAO;
layout(binding = 5) uniform sampler2D in_Bloom;

layout(binding = 6) uniform UBO_Parameters { Parameters u_Parameters; };

layout(location = 0) out vec4 out_Color;

layout(location = 0) in vec2 in_UV;

//#include "Conversions.glsl"

//void DecodeVisibilityBentNormal(const uint packedValue, out float visibility, out vec3 bentNormal )
//{
//    vec4 decoded = R8G8B8A8_UNORM_to_FLOAT4( packedValue );
//    bentNormal = decoded.xyz * 2.0.xxx - 1.0.xxx;   // could normalize - don't want to since it's done so many times, better to do it at the final step only
//    visibility = decoded.w;
//}

void main() {
    vec4 finalImage = texture(in_Color, in_UV).rgba;
    
    float aoVisibility = 1.0;
    uint value = texture(in_SSAO, in_UV).r;
    aoVisibility = value / 255.0;

    // NOTE: currently bent normals are not used but will be...
    // vec3 bentNormal = vec3(0);
    // DecodeVisibilityBentNormal( value, aoVisibility, bentNormal );
    // viewspace to worldspace - makes sense to precalculate
    // bentNormal = mul( (float3x3)g_globals.ViewInv, bentNormal );

    vec4 ssr = texture(in_SSR, in_UV).rgba;
    vec4 bloom = texture(in_Bloom, in_UV);

    if (u_Parameters.EnableSSAO) {
      finalImage *= aoVisibility;
    }
    if (u_Parameters.EnableSSR) {
      finalImage += ssr;
    }
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