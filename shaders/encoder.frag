#version 460

layout (location = 0) in vec2 TexCoord;
layout (location = 0) out vec4 FragColor;
layout (binding = 0) uniform sampler2D input_tex;

layout(push_constant) uniform constants {
    vec2 resolution;
    int frame;
} PushConstants;

// Encoder or Modulator
// This pass converts RGB colors on iChannel0 to
// a YIQ (NTSC) Composite signal.

#define PI   3.14159265358979323846
#define TAU  6.28318530717958647693

const mat3 rgb_to_yiq = mat3(
    0.299,  0.596,  0.211,
    0.587, -0.274, -0.523,
    0.114, -0.322,  0.312
);

float hash11(float p) {
    p = fract(p * .1031);
    p *= p + 33.33;
    p *= p + p;
    return fract(p);
}

float hash12(vec2 p) {
	vec3 p3  = fract(vec3(p.xyx) * .1031);
    p3 += dot(p3, p3.yzx + 33.33);
    return fract((p3.x + p3.y) * p3.z);
}

float hash13(vec3 p3) {
	p3  = fract(p3 * .1031);
    p3 += dot(p3, p3.zyx + 31.32);
    return fract((p3.x + p3.y) * p3.z);
}

void main() {
    vec2 uv = TexCoord * PushConstants.resolution;

    float noise_strength = 0.0;

    // Get a pixel from iChannel0
    // float noise_y = hash12(vec2(float(int(uv.y) >> 1), float(frame))) * noise_strength;
    float noise_x = hash13(vec3(uv.xy, float(PushConstants.frame))) * noise_strength;
    // float noise = noise_x + noise_y;

    // Chroma encoder oscillator frequency 
    float fc = 16.0;

    // Base oscillator angle for this dot
    float t = uv.x;
    
    // Get a pixel from input_texture
    vec3 rgb = texture(input_tex, uv / PushConstants.resolution).rgb;

    // Convert to YIQ
    vec3 yiq = rgb_to_yiq * rgb;
    
    // Final oscillator angle
    float f = fc * t + ((PI / 2.0) * uv.y) + (PI * float((PushConstants.frame >> 1) & 1));

    // Modulate IQ signals
    float i = yiq.y * cos(f), // I signal
          q = yiq.z * sin(f); // Q signal
    
    // Add to Y to get the composite signal
    float c = yiq.x + (i + q); // Composite

    FragColor = vec4(vec3(c), 1.0);
}