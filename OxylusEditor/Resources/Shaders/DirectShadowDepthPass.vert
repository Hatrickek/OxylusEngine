#version 450
#pragma shader_stage(vertex)

layout(location = 0) in vec3 in_Pos;

layout(binding = 0) uniform UBO {
  mat4 projection[4];
  vec4 cascadeSplits;
} u_Ubo;

layout(push_constant) uniform ModelConst {
  mat4 model;
  uint cascadeIndex;
};

out gl_PerVertex { vec4 gl_Position; };

void main() {
	vec4 pos = vec4(in_Pos, 1.0);

	gl_Position = u_Ubo.projection[cascadeIndex] * model* pos;
}