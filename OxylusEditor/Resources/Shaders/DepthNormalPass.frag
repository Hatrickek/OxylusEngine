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

    vec2 scaled_uv = in_UV;
	scaled_uv *= mat.UVScale;

    const float normal_map_strenght = mat.UseNormal ? 1.0 : 0.0;
    vec3 normal = texture(in_NormalMap, scaled_uv).rgb;
    normal = in_WorldTangent * normalize(normal * 2.0 - 1.0);
    normal = mix(normalize(in_Normal), normal, normal_map_strenght);
    normal = normalize((inverse(mat3(view))) * normal);
    
    float inv_roughness = 0.0;
    if (mat.UsePhysicalMap) {
      inv_roughness = texture(in_PhysicalMap, scaled_uv).g;
      inv_roughness *= 1.0 - mat.Roughness;
    } else {
      inv_roughness = 1.0 - mat.Roughness;
    }
    
    out_Normal = vec4(normal, inv_roughness);
}