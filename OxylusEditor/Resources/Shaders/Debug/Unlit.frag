#version 450
#pragma shader_stage(fragment)

layout(location = 0) in vec3 in_Position;
layout(location = 1) in vec4 in_Color;

layout(location = 0) out vec4 out_Color;

void main() {
  vec4 color = in_Color;
  out_Color = color;
}