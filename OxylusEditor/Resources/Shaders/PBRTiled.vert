#version 450

#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable

layout(location = 0) in vec3 in_Pos;
layout(location = 1) in vec3 in_Normal;
layout(location = 2) in vec2 in_UV;

layout(binding = 0) uniform UBO {
  mat4 projection;
  mat4 view;
  vec3 camPos;
}
u_Ubo;

layout(push_constant) uniform ModelConst { mat4 model; }
u_ModelUbo;

layout(location = 0) out vec3 out_WorldPos;
layout(location = 1) out vec3 out_Normal;
layout(location = 2) out vec2 out_UV;
layout(location = 3) out vec3 out_ViewPos;

out gl_PerVertex { vec4 gl_Position; };

void main() {
  vec3 locPos = vec3(u_ModelUbo.model * vec4(in_Pos, 1.0));
  out_WorldPos = locPos;
  out_ViewPos = (u_Ubo.view * vec4(locPos.xyz, 1.0)).xyz;
  out_Normal = mat3(u_ModelUbo.model) * in_Normal;
  out_UV = in_UV;
  out_UV.t = in_UV.t;
  gl_Position = u_Ubo.projection * u_Ubo.view * vec4(out_WorldPos, 1.0);
}
