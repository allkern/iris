#version 330 core

in vec3 VertColor;
in vec2 FragCoord;
out vec4 FragColor;
uniform sampler2D vram;
uniform vec2 screen_size;

vec4 read_vram(int addr) {
    vec2 p = vec2(ceil(addr % 1024), ceil(addr / 1024));

    p /= 1024.0;

    return texture(vram, p);
}

void main() {
    vec2 uv = ((FragCoord + 1.0) * 0.5) * screen_size;

    FragColor = vec4(VertColor, 1.0); // read_vram(int(floor(uv.x)) + (int(floor(uv.y)) * int(floor(screen_size.x))));
}