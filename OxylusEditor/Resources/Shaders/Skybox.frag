#version 450

layout(location = 0) in vec3 inUVW;
layout(location = 0) out vec4 outColor;

layout(binding = 1) uniform samplerCube samplerEnv;

void main() {
  vec3 uv = inUVW;
  const float lodBias = 1.2f;

  vec4 finalColor = textureLod(samplerEnv, uv, lodBias);

  outColor = finalColor;
}