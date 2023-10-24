#version 450
#pragma shader_stage(fragment)

layout(location = 0) in vec3 in_UV;
layout(location = 0) out vec4 out_Color;

layout(binding = 1) uniform samplerCube s_Env;

layout(push_constant) uniform PushConst { 
	mat4 Model; 
	float LodBias;
};

void main() {
  vec3 uv = in_UV;

  vec4 finalColor = textureLod(s_Env, uv, LodBias);

  out_Color = finalColor;
}