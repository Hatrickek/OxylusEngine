#version 450
#pragma shader_stage(vertex)

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
};

#define MAX_NUM_JOINTS 128

layout (set = 3, binding = 0) uniform Node {
	mat4 matrix;
	mat4 joint_matrix[MAX_NUM_JOINTS];
	float joint_count;
} node;

layout(push_constant) uniform ModelConst { mat4 ModelMatrix; };

layout(location = 0) out vec3 out_WorldPos;
layout(location = 1) out vec3 out_Normal;
layout(location = 2) out vec2 out_UV;
layout(location = 3) out mat3 out_WorldTangent;

out gl_PerVertex { vec4 gl_Position; };

void main() {
	vec4 locPos;
	if (node.joint_count > 0.0) {
		// Mesh is skinned
		mat4 skin_mat = in_Weight.x * node.joint_matrix[int(in_Joint.x)] +
						in_Weight.y * node.joint_matrix[int(in_Joint.y)] +
						in_Weight.z * node.joint_matrix[int(in_Joint.z)] +
						in_Weight.w * node.joint_matrix[int(in_Joint.w)];

		locPos = ModelMatrix * node.matrix * skin_mat * vec4(in_Pos, 1.0);
		out_Normal = normalize(transpose(inverse(mat3(ModelMatrix * node.matrix * skin_mat))) * in_Normal);
	} else {
		locPos = ModelMatrix * node.matrix * vec4(in_Pos, 1.0);
		out_Normal = normalize(transpose(inverse(mat3(ModelMatrix * node.matrix))) * in_Normal);
	}

  //vec3 locPos = vec3(model * vec4(in_Pos, 1.0));
  out_WorldPos = locPos.xyz / locPos.w;

  // normal in viewspace
  mat4 view = view;
  //mat3 normalMatrix = transpose(inverse(mat3(ModelMatrix)));
  //out_Normal = normalMatrix * in_Normal;

  out_UV = in_UV;

  vec3 T = normalize((ModelMatrix * (in_Tangent + vec4(0.00001))).xyz);
  vec3 N = normalize((ModelMatrix * vec4(in_Normal, 0.0)).xyz);
  T = normalize(T - dot(T, N) * N);
  vec3 B = cross(N, T);

  out_WorldTangent = mat3(T, B, N);

  gl_Position = projection * view * vec4(out_WorldPos, 1.0);
}