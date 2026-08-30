#version 460

#define A_GLSL 1
#define A_GPU 1
#define FSR_RCAS_F 1

layout (binding = 0) uniform sampler2D input_tex;

layout(push_constant) uniform constants {
    vec2 resolution;
    int frame;
    int pad;
    uvec4 con0;
    uvec4 con1;
    uvec4 con2;
    uvec4 con3;
} PushConstants;

vec4 FsrRcasLoadF(ivec2 p) {
    return texelFetch(input_tex, clamp(p, ivec2(0), ivec2(PushConstants.resolution) - 1), 0);
}

void FsrRcasInputF(inout float r, inout float g, inout float b) {}

#include "ffx_a.h"
#include "ffx_fsr1.h"

layout (location = 0) in vec2 TexCoord;
layout (location = 0) out vec4 FragColor;

void main() {
    vec3 color;

    FsrRcasF(color.r, color.g, color.b,
        uvec2(TexCoord * PushConstants.resolution),
        PushConstants.con0);

    FragColor = vec4(color, 1.0);
}
