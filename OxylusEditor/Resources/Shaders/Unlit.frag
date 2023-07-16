#version 450

layout(location = 0) in vec3 in_Position;
layout(location = 2) in vec2 in_TexCoord;
layout(location = 3) in vec4 in_Color;

layout(location = 0) out vec4 out_Color;

void main() {
  vec4 color = in_Color;
  if (color.a < 0.1) {
    discard;
  }
  out_Color = color;
}