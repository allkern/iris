#version 460

layout (location = 0) in vec4 in_rgba;
layout (location = 1) in vec2 in_uv;
layout (location = 2) in vec3 in_stq;
layout (location = 0) out vec4 FragColor;

void main() {
    FragColor = in_rgba;
}