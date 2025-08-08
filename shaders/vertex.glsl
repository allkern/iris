#version 460

layout (location = 0) in vec2 position;
layout (location = 1) in vec2 in_uv;
layout (location = 0) out vec2 out_uv;

void main() {
    gl_Position = vec4(position, 0.0f, 1.0f);
    out_uv = vec2(in_uv.x, 1.0 - in_uv.y);
}