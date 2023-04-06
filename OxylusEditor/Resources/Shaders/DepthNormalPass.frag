#version 450

layout(set = 1, binding = 1) uniform sampler2D in_NormalSampler;

layout(location = 0) in vec3 in_WorldPos;
layout(location = 1) in vec3 in_Normal;
layout(location = 2) in vec2 in_UV;
layout(location = 3) in mat3 in_WorldTangent;

layout(location = 0) out vec4 out_Normal;

layout(push_constant, std140) uniform Material {
  layout(offset = 64) vec4 color;
  layout(offset = 80) float roughness;
  layout(offset = 84) float metallic;
  layout(offset = 88) float specular;
  layout(offset = 92) float normal;
  layout(offset = 96) float ao;
  layout(offset = 100) bool useAlbedo;
  layout(offset = 104) bool useRoughness;
  layout(offset = 108) bool useMetallic;
  layout(offset = 112) bool useNormal;
  layout(offset = 116) bool useAO;
}
u_Material;

void main() {
  const float normalMapStrenght = u_Material.useNormal ? 1.0 : 0.0;
  vec3 normal = texture(in_NormalSampler, in_UV).rgb;
  normal = in_WorldTangent * normalize(normal * 2.0 - 1.0);
  normal = normalize(mix(normalize(in_Normal), normal, normalMapStrenght));
  float roughness = 1.0 - u_Material.roughness;
  out_Normal = vec4(normal, roughness);
}