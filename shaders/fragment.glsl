#version 460

layout (location = 0) in vec2 TexCoord;
layout (location = 0) out vec4 FragColor;
layout (set = 2, binding = 0) uniform sampler2D input_tex;

void main() {
    FragColor = texture(input_tex, TexCoord).zyxw;
}