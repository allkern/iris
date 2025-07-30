#version 460

layout (location = 2) in uvec3 xyz;
layout (location = 0) in uvec4 in_rgba;
layout (location = 1) in uvec2 in_uv;
layout (location = 3) in vec3 in_stq;

layout (location = 0) out vec4 out_rgba;
layout (location = 1) out vec2 out_uv;
layout (location = 2) out vec3 out_stq;

void main() {
    vec2 ndc = ((vec3(xyz).xy / vec2(640.0, 448.0)) * 2.0) - 1.0;

    float z = float(xyz.z & 0xffffff) / 16777215.0f;

    gl_Position = vec4(ndc, z, 1.0);

    out_uv = vec2(in_uv.x, 1.0 - in_uv.y);
    out_rgba = in_rgba / vec4(255.0);
    out_stq = in_stq;
}