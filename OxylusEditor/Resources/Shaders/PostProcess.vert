#version 450

layout(location = 0) in vec3 in_Position;
layout(location = 2) in vec2 in_TexCoord;

layout(location = 1) out vec2 out_TexCoord;

void main() {
  out_TexCoord = in_TexCoord;
  gl_Position = vec4(in_Position.xy, 0.0, 1.0);
}