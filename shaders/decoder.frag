#version 460

layout (location = 0) in vec2 TexCoord;
layout (location = 0) out vec4 FragColor;
layout (binding = 0) uniform sampler2D input_tex;

layout(push_constant) uniform constants {
    vec2 resolution;
    int frame;
} PushConstants;

// Decoder or Demodulator
// This pass takes the Composite signal generated on Buffer B
// and decodes it

#define PI   3.14159265358979323846
#define TAU  6.28318530717958647693

#define BRIGHTNESS_FACTOR        2.0

// The decoded IQ signals get multiplied by this
// factor. Bigger values yield more color saturation
#define CHROMA_SATURATION_FACTOR 3.0

// Size of the decoding FIR filter. bigger values
// yield more smuggly video and are more expensive
#define CHROMA_DECODER_FIR_SIZE  8
#define LUMA_DECODER_FIR_SIZE  4

float sinc(float x) {
    x *= PI;
    
    return (x == 0.0) ? 1.0 : (sin(x) / x);
}

float sincf(float cutoff, float n) {
    float cut2 = 2.0 * cutoff;
    
    return cut2 * sinc(cut2 * n);
}

// YIQ to RGB matrix
const mat3 yiq_to_rgb = mat3(
    1.000,  1.000,  1.000,
    0.956, -0.272, -1.106,
    0.621, -0.647,  1.703
);

float blackman(float n, float N) {
    float a0 = (1.0 - 0.16) / 2.0;
    float a1 = 1.0 / 2.0;
    float a2 = 0.16 / 2.0;
    
    return a0 - (a1 * cos((2.0 * PI * n) / N)) + (a2 * cos((4.0 * PI * n) / N)) * 1.0;
}

void main() {
    vec2 uv = TexCoord * PushConstants.resolution;

    // Chroma decoder oscillator frequency 
    float fc = 16.0;

    float counter = 0.0;

    // Sum and decode NTSC samples
    // This is essentially a simple averaging filter
    // that happens to be weighted by two cos and sin
    // oscillators at a very specific frequency
    vec3 yiq = vec3(0.0);
    
    // Decode Luma first
    for (int d = -LUMA_DECODER_FIR_SIZE; d < LUMA_DECODER_FIR_SIZE; d++) {
        vec2 p = vec2(uv.x + float(d), uv.y);
        vec3 s = texture(input_tex, p / PushConstants.resolution).rgb;
        float t = fc * (uv.x + float(d));

        float window = blackman(float(d + LUMA_DECODER_FIR_SIZE), float(LUMA_DECODER_FIR_SIZE)); 
        float filt = sincf(0.25, float(d)) * 2.75;

        yiq.x += s.x * filt;
    }

    yiq.x /= LUMA_DECODER_FIR_SIZE;
    yiq.x *= BRIGHTNESS_FACTOR;
    
    // Then decode chroma
    for (int d = -CHROMA_DECODER_FIR_SIZE; d < 0; d++) {
        vec2 p = vec2(uv.x + float(d), uv.y);
        vec3 s = texture(input_tex, p / PushConstants.resolution).rgb;
        float t = fc * (uv.x + float(d)) + ((PI / 2.0) * uv.y) + (PI * float((PushConstants.frame >> 1) & 1));

        // Apply Blackman window for smoother colors
        float window = blackman(float(d + CHROMA_DECODER_FIR_SIZE), float(CHROMA_DECODER_FIR_SIZE)); 
        float filt = sincf(0.25, float(d));

        yiq.yz += s.yz * vec2(cos(t), sin(t));
    }

    yiq.yz /= (CHROMA_DECODER_FIR_SIZE * 2) + 1;
    yiq.yz *= CHROMA_SATURATION_FACTOR;

    FragColor = vec4((yiq_to_rgb * yiq), 1.0);
}