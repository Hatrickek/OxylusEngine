#version 450
#pragma shader_stage(fragment)

layout(location = 0) in vec3 in_UV;
layout(location = 0) out vec4 out_Color;

layout(binding = 1) uniform samplerCube s_Env;

layout(push_constant) uniform PushConst { 
	mat4 Model; 
};

void main() {
  vec3 uv = in_UV;

  vec4 finalColor = texture(s_Env, uv);

  out_Color = finalColor;
}