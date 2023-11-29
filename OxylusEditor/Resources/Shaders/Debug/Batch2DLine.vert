#version 450
#pragma shader_stage(vertex)

layout (location = 0) in vec4 in_Pos;
layout (location = 1) in vec4 in_Color;

layout(set = 0,binding = 0) uniform UBO {
	mat4 view_proj;
};

layout (location = 0) out vec3 out_Pos;
layout (location = 1) out vec4 out_Color;

void main() {
	out_Pos = in_Pos.xyz;
	out_Color = in_Color;
	gl_Position =  view_proj * vec4(out_Pos, 1.0);
}