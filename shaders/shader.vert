#version 460

layout (location = 0) out vec2 out_uv;

const vec2 vertices[4] = vec2[](
    vec2(-1.0, -1.0),
    vec2( 1.0, -1.0),
    vec2( 1.0,  1.0),
    vec2(-1.0,  1.0)
);

const vec2 uvs[4] = vec2[](
    vec2(0.0, 1.0),
    vec2(1.0, 1.0),
    vec2(1.0, 0.0),
    vec2(0.0, 0.0)
);

void main() {
    gl_Position = vec4(vertices[gl_VertexIndex], 0.0f, 1.0f);

    out_uv = uvs[gl_VertexIndex];
    out_uv.y = 1.0 - out_uv.y;
}