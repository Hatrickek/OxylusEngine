#version 450

// layout (binding = 0) uniform sampler2D u_Texture;

layout(location = 0) in vec3 in_Position;
layout(location = 2) in vec2 in_TexCoord;
layout(location = 3) in vec4 in_Color;

layout(location = 0) out vec4 out_Color;

void main() {
  vec4 color = in_Color; //* texture(u_Texture, in_TexCoord);
  if (color.a < 0.1) {   // TODO: Temporary until actual blending
    discard;
  }
  out_Color = color;
}