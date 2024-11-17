#version 330 core

layout (location = 0) in vec3 pos;
layout (location = 1) in vec3 col;

uniform vec2 screen_size;
out vec3 VertColor;
out vec2 FragCoord;

void main() {
    vec2 p = pos.xy;
    p.x = ((pos.x * 2.0) - screen_size.x) / screen_size.x;
    p.y = (screen_size.y - (pos.y * 2.0)) / screen_size.y;

    FragCoord = p;
    FragCoord.y = (2.0 - (FragCoord.y + 1.0)) - 1.0;

    VertColor = col / 255.0;

    gl_Position = vec4(p, 0.0, 1.0);
}