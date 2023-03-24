#version 450

layout(location = 0) in vec3 in_Position;
layout(location = 2) in vec2 in_TexCoord;
layout(location = 3) in vec4 in_Color;

layout(location = 0) out vec3 out_Position;
layout(location = 2) out vec2 out_TexCoord;
layout(location = 3) out vec4 out_Color;

layout(push_constant) uniform UBO {
  mat4 projection;
  mat4 view;
}
u_Ubo;

void main() {
  out_TexCoord = in_TexCoord;
  out_Position = in_Position;
  out_Color = in_Color;
  gl_Position = (u_Ubo.projection * u_Ubo.view) * vec4(in_Position, 1.0);
}