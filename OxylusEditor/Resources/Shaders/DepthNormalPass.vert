#version 450

#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable

layout(location = 0) in vec3 inPos;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inUV;

layout(binding = 0) uniform UBO {
  mat4 projection;
  mat4 view;
  vec3 camPos;
}
u_Ubo;

layout(push_constant) uniform ModelConst { mat4 model; }
u_ModelUbo;

layout(location = 0) out vec3 outWorldPos;
layout(location = 1) out vec3 outNormal;
layout(location = 2) out vec2 outUV;

out gl_PerVertex { vec4 gl_Position; };

void main() {
  vec3 locPos = vec3(u_ModelUbo.model * vec4(inPos, 1.0));
  outWorldPos = locPos;

  // normal in viewspace
  mat4 view = u_Ubo.view;
  mat3 normalMatrix = transpose(inverse(mat3(view * u_ModelUbo.model)));
  outNormal = normalMatrix * inNormal;

  outUV = inUV;

  gl_Position = u_Ubo.projection * u_Ubo.view * vec4(outWorldPos, 1.0);
}