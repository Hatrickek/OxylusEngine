#version 450
#pragma shader_stage(vertex)

layout(location = 0) in vec3 inPos;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inUV;
layout(location = 3) in vec4 inTangent;

layout(binding = 0) uniform UBO {
  mat4 projection;
  mat4 view;
  vec3 camPos;
};

layout(push_constant) uniform ModelConst { mat4 model; };

layout(location = 0) out vec3 outWorldPos;
layout(location = 1) out vec3 outNormal;
layout(location = 2) out vec2 outUV;
layout(location = 3) out mat3 outWorldTangent;

out gl_PerVertex { vec4 gl_Position; };

void main() {
  vec3 locPos = vec3(model * vec4(inPos, 1.0));
  outWorldPos = locPos;

  // normal in viewspace
  mat4 view = view;
  mat3 normalMatrix = transpose(inverse(mat3(model)));
  outNormal = normalMatrix * inNormal;

  outUV = inUV;

  vec3 T = normalize((model * (inTangent + vec4(0.00001))).xyz);
  vec3 N = normalize((model * vec4(inNormal, 0.0)).xyz);
  T = normalize(T - dot(T, N) * N);
  vec3 B = cross(N, T);

  outWorldTangent = mat3(T, B, N);

  gl_Position = projection * view * vec4(outWorldPos, 1.0);
}