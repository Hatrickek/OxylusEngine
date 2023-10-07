#version 450
#pragma shader_stage(fragment)

layout(binding = 0) uniform UBO {
  mat4 projection;
  mat4 view;
  vec3 camPos;
};

layout(set = 1, binding = 0) uniform sampler2D in_NormalMap;
layout(set = 1, binding = 1) uniform sampler2D in_PhysicalMap;

layout(location = 0) in vec3 in_WorldPos;
layout(location = 1) in vec3 in_Normal;
layout(location = 2) in vec2 in_UV;
layout(location = 3) in mat3 in_WorldTangent;

layout(location = 0) out vec4 out_Normal;

#include "Material.glsl"

void main() {
    Material mat = Materials[MaterialIndex];

    const float normalMapStrenght = mat.UseNormal ? 1.0 : 0.0;
    vec3 normal = texture(in_NormalMap, in_UV).rgb;
    normal = in_WorldTangent * normalize(normal * 2.0 - 1.0);
    normal = normalize(mix(normalize(in_Normal), normal, normalMapStrenght));
    normal = normalize(mat3(view) * normal);
    
    float roughness = 1.0;
    if (mat.UsePhysicalMap) {
      roughness = 1.0 - texture(in_PhysicalMap, in_UV * mat.UVScale).g;
      roughness *= mat.Roughness;
    } else {
      roughness = 1.0 - mat.Roughness;
    }
    
    out_Normal = vec4(normalize(normal), roughness);
}