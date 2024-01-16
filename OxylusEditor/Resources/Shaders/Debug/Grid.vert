#version 450
#pragma shader_stage(vertex)

layout(location = 0) in vec4 in_Pos;
layout(location = 1) in vec4 in_Normal;
layout(location = 2) in vec2 in_UV;

layout(set = 0, binding = 0) uniform UBO {
  mat4 view;
  mat4 proj;
}
ubo;

layout(location = 0) out vec3 out_Position;
layout(location = 1) out vec2 out_UV;
layout(location = 2) out vec3 out_NearPoint;
layout(location = 3) out vec3 out_FarPoint;
layout(location = 4) out mat4 out_FragView;
layout(location = 8) out mat4 out_FragProj;

out gl_PerVertex { vec4 gl_Position; };

vec3 unproject_point(float x, float y, float z, mat4 view, mat4 projection) {
  mat4 viewInv = inverse(view);
  mat4 projInv = inverse(projection);
  vec4 unprojectedPoint = viewInv * projInv * vec4(x, y, z, 1.0);
  return unprojectedPoint.xyz / unprojectedPoint.w;
}

void main() {
  vec4 position = in_Pos;
  out_Position = in_Pos.xyz;

  vec3 normal = in_Normal.xyz;

  out_FragView = ubo.view;
  out_FragProj = ubo.proj;

  vec3 p = position.xyz;
  out_NearPoint = unproject_point(p.x, p.y, 0.0, ubo.view, ubo.proj).xyz;
  out_FarPoint = unproject_point(p.x, p.y, 1.0, ubo.view, ubo.proj).xyz;

  vec2 uv = in_UV;
  out_UV = uv;
  gl_Position = vec4(p.xyz, 1.0);
}