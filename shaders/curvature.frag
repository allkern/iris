#version 460

layout (location = 0) in vec2 TexCoord;
layout (location = 0) out vec4 FragColor;
layout (binding = 0) uniform sampler2D input_tex;

layout(push_constant) uniform constants {
    vec2 resolution;
    int frame;
} PushConstants;

#define PI 3.14159265358979323846
#define FIR_WIDTH 1

vec2 curvature(vec2 coord, float bend) {
	// put in symmetrical coords
	coord = (coord - 0.5) * 2.0;
	coord *= 1.1;	

	// deform coords
	coord.x *= 1.0 + pow((abs(coord.y) / bend), 2.0);
	coord.y *= 1.0 + pow((abs(coord.x) / bend), 2.0);

	// transform back to 0.0 - 1.0 space
	coord  = (coord / 2.0) + 0.5;

	return coord;
}

const float a0 = 0.215578950;
const float a1 = 0.416631580;
const float a2 = 0.277263158;
const float a3 = 0.083578947;
const float a4 = 0.006947368;

float flat_top(float x, float n) {
    return a0 - (a1 * cos((2.0 * PI * x) / n)) + (a2 * cos((4.0 * PI * x) / n)) - (a3 * cos((6.0 * PI * x) / n)) + (a4 * cos((8.0 * PI * x) / n));
}

const float freq = 9.0;

float sinc(float x) {
    return sin(freq * PI * (x - 0.5)) / (freq * PI * (x - 0.5));
}

void main() {
    vec2 uv = curvature(TexCoord, 5.5);

	if (uv.x < 0.0 || uv.x > 1.0 || uv.y < 0.0 || uv.y > 1.0) {
        FragColor = vec4(0.0);

        return;
    }

    FragColor = texture(input_tex, uv).xyzw;
}