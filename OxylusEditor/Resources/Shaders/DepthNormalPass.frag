#version 450

layout(location = 1) in vec3 in_Normal;

layout(location = 0) out vec4 out_Normal;

void main() {
  vec4 in_Normal = vec4((normalize(in_Normal) * 0.5) + vec3(0.5), 1.0);
  out_Normal = in_Normal;
}