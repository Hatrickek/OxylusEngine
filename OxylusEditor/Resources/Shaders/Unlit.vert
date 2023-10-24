#version 450
#pragma shader_stage(vertex)

layout(location = 0) in vec3 in_Position;
layout(location = 1) in vec3 in_Normal;
layout(location = 2) in vec2 in_TexCoord;

layout(location = 0) out vec3 out_Position;
layout(location = 1) out vec2 out_TexCoord;
layout(location = 2) out vec4 out_Color;

layout(push_constant) uniform UBO {
  mat4 MVP;
  vec4 Color;
}
u_PC;

void main() {
  out_TexCoord = in_TexCoord;
  out_Position = in_Position;
  out_Color = vec4(1);//u_PC.Color;
  gl_Position = u_PC.MVP * vec4(in_Position, 1.0);
}