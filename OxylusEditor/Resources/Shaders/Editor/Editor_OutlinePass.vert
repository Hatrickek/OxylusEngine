#version 450
#pragma shader_stage(vertex)

layout(location = 0) in vec3 in_Pos;
layout(location = 1) in vec3 in_Normal;

layout(binding = 0) uniform UBO {
  mat4 projection_view;
};

layout(push_constant) uniform ModelConst { 
	mat4 model_matrix;
	float outline_width;
};

layout(location = 0) out vec3 out_Color;

out gl_PerVertex { vec4 gl_Position; };

void main() {
	out_Color = vec3(0, 1, 0);

	// Extrude along normal
	vec4 pos = vec4(in_Pos.xyz /* + in_Normal * outline_width */, 1.0);
	gl_Position = projection_view * model_matrix * pos;
}