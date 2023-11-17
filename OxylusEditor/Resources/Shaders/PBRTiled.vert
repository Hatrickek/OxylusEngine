#version 450
#pragma shader_stage(vertex)

#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable

layout(location = 0) in vec3 in_Pos;
layout(location = 1) in vec3 in_Normal;
layout(location = 2) in vec2 in_UV;
layout(location = 3) in vec4 in_Tangent;
layout(location = 4) in vec4 in_Color;
layout(location = 5) in vec4 in_Joint;
layout(location = 6) in vec4 in_Weight;

layout(binding = 0) uniform UBO {
	mat4 projection;
	mat4 view;
	vec3 camPos;
} u_Ubo;

#ifdef ANIMATIONS
#define MAX_NUM_JOINTS 128
layout (set = 3, binding = 0) uniform Node {
	mat4 joint_matrix[MAX_NUM_JOINTS];
	float joint_count;
} node;
#endif

layout(push_constant) uniform ModelConst { mat4 ModelMatrix; };

layout(location = 0) out vec3 out_WorldPos;
layout(location = 1) out vec3 out_Normal;
layout(location = 2) out vec2 out_UV;
layout(location = 3) out vec3 out_ViewPos;

out gl_PerVertex { vec4 gl_Position; };

void main() {
#ifdef ANIMATIONS // TODO: move to a new shader
	if (node.joint_count > 0.0) {
		// Mesh is skinned
		mat4 skin_mat = in_Weight.x * node.joint_matrix[int(in_Joint.x)] +
						in_Weight.y * node.joint_matrix[int(in_Joint.y)] +
						in_Weight.z * node.joint_matrix[int(in_Joint.z)] +
						in_Weight.w * node.joint_matrix[int(in_Joint.w)];

		locPos = ModelMatrix * skin_mat * vec4(in_Pos, 1.0);
		out_Normal = normalize(transpose(inverse(mat3(ModelMatrix * skin_mat))) * in_Normal);
	} 
	else 
#endif
	vec4 locPos = ModelMatrix * vec4(in_Pos, 1.0);
	out_Normal = normalize(transpose(inverse(mat3(ModelMatrix))) * in_Normal);

	//vec4 locPos = ModelMatrix * vec4(in_Pos, 1.0);
	out_WorldPos = locPos.xyz / locPos.w;
	out_ViewPos = (u_Ubo.view * vec4(locPos.xyz, 1.0)).xyz;
	//out_Normal = normalize(transpose(inverse(mat3(ModelMatrix))) * in_Normal);
	out_UV = in_UV;
	gl_Position = u_Ubo.projection * u_Ubo.view * vec4(out_WorldPos, 1.0);
}
