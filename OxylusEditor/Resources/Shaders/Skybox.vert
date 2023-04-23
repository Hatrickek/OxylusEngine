#version 450

#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable

layout(location = 0) in vec3 inPos;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inUV;

layout(binding = 0) uniform UBO { mat4 projection; }
u_Ubo;

layout(push_constant) uniform ViewConst { mat4 model; }
u_ModelUbo;

layout(location = 0) out vec3 outUVW;

out gl_PerVertex { vec4 gl_Position; };

void main() {
  outUVW = inPos;
  gl_Position = u_Ubo.projection * u_ModelUbo.model * vec4(inPos.xyz, 1.0);
}