#version 450
#pragma shader_stage(vertex)

layout(location = 0) in vec3 in_Pos;

layout(binding = 0) uniform UBO {
  mat4 projection_view;
};

layout(push_constant) uniform ModelConst { 
	mat4 ModelMatrix; 
	uint EntityID;
};

layout(location = 0) out vec3 out_WorldPos;
layout(location = 1) out uint out_EntityID;

out gl_PerVertex { vec4 gl_Position; };

void main() {
	vec4 locPos = ModelMatrix * vec4(in_Pos, 1.0);
	out_WorldPos = locPos.xyz / locPos.w;

	out_EntityID = EntityID;

	gl_Position = projection_view * vec4(out_WorldPos, 1.0);
}