#version 450
#pragma shader_stage(vertex)

layout(location = 0) in vec3 in_Position;
layout(location = 1) in vec3 in_Normal;
layout(location = 2) in vec2 in_TexCoord;

layout(location = 0) out vec3 out_Position;
layout(location = 1) out vec2 out_TexCoord;
layout(location = 2) out vec4 out_Color;

struct Buffer {
  mat4 view_projection;
  mat4 model_matrix;
  vec4 color;
};

layout(std140, set = 0, binding = 0) readonly buffer MaterialBuffer {
	Buffer u_buffers[];
};

layout(push_constant) uniform BufferIndex { int buffer_index; };

void main() {
	vec4 local_pos = u_buffers[buffer_index].model_matrix * vec4(in_Position, 1.0);

	vec3 world_pos = local_pos.xyz / local_pos.w;

	out_TexCoord = in_TexCoord;
	out_Position = in_Position;
	out_Color = u_buffers[buffer_index].color;
	gl_Position = u_buffers[buffer_index].view_projection * vec4(world_pos, 1.0);
}