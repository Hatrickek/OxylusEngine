#version 450
#pragma shader_stage(vertex)

layout(location = 0) in vec3 in_Pos;
layout(location = 1) in vec3 in_Normal;
layout(location = 2) in vec2 in_UV;

layout(binding = 0) uniform UBO {
  mat4 projection[4];
  vec4 cascadeSplits;
} u_Ubo;

layout(push_constant) uniform ModelConst {
  mat4 model;
  uint cascadeIndex;
};

layout(location = 0) out vec3 out_Pos;
layout(location = 1) out vec3 out_Normal;
layout(location = 2) out vec2 out_UV;

out gl_PerVertex { vec4 gl_Position; };

void main() {
	vec3 pos = in_Pos;
	vec3 normal = in_Normal;
	out_UV = in_UV;
	out_Pos = in_Pos;

	gl_Position = u_Ubo.projection[cascadeIndex] * model * vec4(pos, 1.0);
}