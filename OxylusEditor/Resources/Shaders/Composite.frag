#version 450
#pragma shader_stage(fragment)

layout(location = 0) in vec3 in_Position;
layout(location = 2) in vec2 in_TexCoord;
layout(location = 3) in vec4 in_Color;

layout(binding = 0) uniform sampler2D u_Color;

layout(location = 0) out vec4 out_Color;

void main() {
  vec4 color = in_Color;
  if (color.a < 0.1) {
    discard;
  }
  color += texture(u_Color, in_TexCoord);

  out_Color = color;
}