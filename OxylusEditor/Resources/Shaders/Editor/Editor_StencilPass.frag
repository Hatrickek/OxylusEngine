#version 450
#pragma shader_stage(fragment)

layout(location = 0) in vec3 in_Color;

layout(location = 0) out vec4 out_Color;

void main() {
    out_Color = vec4(0.0);
}