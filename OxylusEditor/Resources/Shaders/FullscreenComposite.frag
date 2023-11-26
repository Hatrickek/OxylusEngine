#version 450
#pragma shader_stage(fragment)

layout(binding = 0) uniform sampler2D u_Sampler;
layout(binding = 1) uniform sampler2D u_Sampler2;

layout(location = 0) in vec2 in_UV;

layout(location = 0) out vec4 outFragColor;

void main() {
	vec4 result = texture(u_Sampler, in_UV);
	vec4 result2 = texture(u_Sampler2, in_UV);
	result += result2;
	outFragColor = result;
}