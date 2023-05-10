#version 450

#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable

layout(location = 0) in vec3 in_Pos;
layout(location = 2) in vec2 in_UV;

layout(binding = 0) uniform UBO {
  mat4 projection[4];
  vec4 cascadeSplits;
}
u_Ubo;

layout(push_constant) uniform ModelConst {
  mat4 model;
  uint cascadeIndex;
}
u_ModelUbo;

layout(location = 0) out vec3 out_Pos;
layout(location = 2) out vec2 out_UV;

out gl_PerVertex { vec4 gl_Position; };

void main() {
  out_UV = in_UV;
  out_Pos = in_Pos;

  mat4 projection = u_Ubo.projection[u_ModelUbo.cascadeIndex];

  gl_Position = projection * u_ModelUbo.model * vec4(in_Pos, 1.0);
}