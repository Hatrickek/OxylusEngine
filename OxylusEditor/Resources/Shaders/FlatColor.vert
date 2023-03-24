#version 450

layout(location = 0) in vec3 in_Position;
layout(location = 2) in vec2 in_TexCoord;

layout(location = 0) out vec3 out_Position;
layout(location = 1) out vec2 out_TexCoord;

void main() {
  out_TexCoord = in_TexCoord;
  out_Position = in_Position;
  gl_Position = vec4(in_Position, 1.0);
}