#version 450
#pragma shader_stage(fragment)

layout(location = 0) in vec3 in_Position;
layout(location = 1) in vec2 in_TexCoord;
layout(location = 2) in vec4 in_Color;

layout(location = 0) out vec4 out_Color;

void main() {
  vec4 color = in_Color;
  //if (color.a < 0.1) {
  //  discard;
  //}
  out_Color = vec4(1, 1, 0, 1);
}