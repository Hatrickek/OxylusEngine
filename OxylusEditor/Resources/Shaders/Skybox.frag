#version 450

layout(binding = 6) uniform samplerCube samplerEnv;

layout(location = 0) in vec3 inUVW;

layout(location = 0) out vec4 outColor;

layout(binding = 1) uniform UBOParams {
  int tonemapper; // 0- Aces 1- Uncharted 2-Filmic 3- Reinhard
  float exposure;
  float gamma;
}
u_UBOParams;

#include "Tonemaps.glsl"

void main() {
  vec3 uv = inUVW;
  const float lodBias = 1.0f;
  vec3 finalColor = textureLod(samplerEnv, uv, lodBias).rgb;

  outColor = vec4(finalColor, 1.0);
}