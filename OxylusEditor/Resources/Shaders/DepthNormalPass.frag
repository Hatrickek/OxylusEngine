#version 450

layout(set = 1, binding = 1) uniform sampler2D in_NormalSampler;
layout(set = 1, binding = 4) uniform sampler2D in_RoughnessSampler;

layout(location = 0) in vec3 in_WorldPos;
layout(location = 1) in vec3 in_Normal;
layout(location = 2) in vec2 in_UV;
layout(location = 3) in mat3 in_WorldTangent;

layout(location = 0) out vec4 out_Normal;

#include "Material.glsl"

void main() {
  const float normalMapStrenght = u_Material.UseNormal ? 1.0 : 0.0;
  vec3 normal = texture(in_NormalSampler, in_UV).rgb;
  normal = in_WorldTangent * normalize(normal * 2.0 - 1.0);
  normal = normalize(mix(normalize(in_Normal), normal, normalMapStrenght));

  float roughness = 0;
  if (u_Material.UseRoughness) {
    roughness = 1.0 - texture(in_RoughnessSampler, in_UV * u_Material.UVScale).r;
    roughness *= u_Material.Roughness;
  } else {
    // roughness = 1.0 - u_Material.Roughness;
  }

  out_Normal = vec4(normal, roughness);
}