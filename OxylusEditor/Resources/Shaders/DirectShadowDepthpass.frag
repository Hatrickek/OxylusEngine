#version 450

layout(location = 0) in vec2 inUV;

layout(set = 1, binding = 0) uniform sampler2D u_Sampler;

void main() {
  vec4 color = texture(u_Sampler, inUV);
  if (color.a < 0.5f) {
    // discard;
  }
}