#version 450
#pragma shader_stage(vertex)

layout(location = 0) in vec3 inPos;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inUV;

layout(binding = 0) uniform UBO { mat4 Projection; };

layout(push_constant) uniform PushConst { 
	mat4 Model; 
	float LodBias;
};

layout(location = 0) out vec3 outUVW;

out gl_PerVertex { vec4 gl_Position; };

void main() {
  outUVW = inPos;
  gl_Position = Projection * Model * vec4(inPos.xyz, 1.0);
}