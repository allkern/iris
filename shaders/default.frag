#version 460

layout (location = 0) in vec2 TexCoord;
layout (location = 0) out vec4 FragColor;
layout (binding = 0) uniform sampler2D input_tex;

void main() {
    FragColor = vec4(texture(input_tex, TexCoord).xyz, 1.0);
}