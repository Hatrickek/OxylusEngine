#version 450
#pragma shader_stage(vertex)

layout(location = 0) in vec4 in_Position;
layout(location = 1) in vec4 in_Color;

layout(location = 0) out vec3 out_Position;
layout(location = 1) out vec4 out_Color;

struct Buffer {
  mat4 view_projection;
  mat4 model_matrix;
};

layout(std140, set = 0, binding = 0) readonly buffer MeshBuffers {
	Buffer u_buffers[];
};

layout(push_constant) uniform NodeBuffer { 
	int buffer_index;
};

void main() {
	vec3 pos = in_Position.xyz;
	vec3 color = in_Color.rgb;

	vec4 local_pos = u_buffers[buffer_index].model_matrix * vec4(pos, 1.0);

	vec3 world_pos = local_pos.xyz / local_pos.w;

	out_Color = vec4(color, 1.0);
	out_Position = pos;
	gl_Position = u_buffers[buffer_index].view_projection * vec4(world_pos, 1.0);
}