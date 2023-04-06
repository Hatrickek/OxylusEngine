#version 450

#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable

layout(location = 0) in vec3 inPos;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inUV;
layout(location = 3) in vec4 inTangent;

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
layout(location = 3) out mat3 outWorldTangent;

out gl_PerVertex { vec4 gl_Position; };

void main() {
  vec3 locPos = vec3(u_ModelUbo.model * vec4(inPos, 1.0));
  outWorldPos = locPos;

  // normal in viewspace
  mat4 view = u_Ubo.view;
  mat3 normalMatrix = transpose(inverse(mat3(u_ModelUbo.model)));
  outNormal = normalMatrix * inNormal;

  outUV = inUV;

  vec3 T = normalize((u_ModelUbo.model * inTangent).xyz);
  vec3 N = normalize((u_ModelUbo.model * vec4(inNormal, 0.0)).xyz);
  T = normalize(T - dot(T, N) * N);
  vec3 B = cross(N, T);

  outWorldTangent = mat3(T, B, N);

  gl_Position = u_Ubo.projection * u_Ubo.view * vec4(outWorldPos, 1.0);
}