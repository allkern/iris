#version 330 core

in vec3 VertColor;
in vec2 FragCoord;
out vec4 FragColor;
uniform sampler2D vram;
uniform vec2 screen_size;

vec4 read_vram(int addr) {
    vec2 p = vec2(float(addr % 1024), float(addr / 1024));

    p /= 1024.0;

    return texture(vram, p);
}

void main() {
    vec2 uv = (FragCoord + 1.0) * 0.5;
    uv.x *= 640.0;
    uv.y *= 480.0;

    FragColor = read_vram(int(uv.x) + (int(uv.y) * 640));
}