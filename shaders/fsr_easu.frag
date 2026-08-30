#version 460

#define A_GLSL 1
#define A_GPU 1
#define FSR_EASU_F 1

layout (binding = 0) uniform sampler2D input_tex;

vec4 FsrEasuRF(vec2 p) { return textureGather(input_tex, p, 0); }
vec4 FsrEasuGF(vec2 p) { return textureGather(input_tex, p, 1); }
vec4 FsrEasuBF(vec2 p) { return textureGather(input_tex, p, 2); }

#include "ffx_a.h"
#include "ffx_fsr1.h"

layout (location = 0) in vec2 TexCoord;
layout (location = 0) out vec4 FragColor;

layout(push_constant) uniform constants {
    vec2 resolution;
    int frame;
    int pad;
    uvec4 con0;
    uvec4 con1;
    uvec4 con2;
    uvec4 con3;
} PushConstants;

void main() {
    vec3 color;

    FsrEasuF(color,
        uvec2(TexCoord * PushConstants.resolution),
        PushConstants.con0,
        PushConstants.con1,
        PushConstants.con2,
        PushConstants.con3);

    FragColor = vec4(color, 1.0);
}
