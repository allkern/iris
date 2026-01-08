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
#define CHROMA_SATURATION_FACTOR 2.0

// Size of the decoding FIR filter. bigger values
// yield more smudgy video and are more expensive
#define CHROMA_DECODER_FIR_SIZE  8
#define LUMA_DECODER_FIR_SIZE  4

float blackman(float n, float N) {
    return 0.42 - 0.5 * cos((2.0 * PI * n) / (N - 1.0)) +
           0.08 * cos((4.0 * PI * n) / (N - 1.0));
}

float sinc(float x) {
    if (x == 0.0) return 1.0;

    float pix = PI * x;

    return sin(pix) / pix;
}

float sinc_f(float cutoff, float n, float N) {
    float cut2 = cutoff * 2.0;

    return cut2 * sinc(cut2 * (n - ((N - 1.0) / 2.0))) * 48.0;
}

float lowpass(float cutoff, float n, float N) {
    return (sinc_f(cutoff, n, N) * 2.0 - 2.0) * blackman(n, N);
}

float highpass(float cutoff, float n, float N) {
    return -(sinc_f(cutoff, n, N) * blackman(n, N));
}

// YIQ to RGB matrix
const mat3 yiq_to_rgb = mat3(
    1.000,  1.000,  1.000,
    0.956, -0.272, -1.106,
    0.621, -0.647,  1.703
);

void main() {
    vec2 uv = TexCoord * PushConstants.resolution;

    // Chroma decoder oscillator frequency 
    float fc = 128.0;

    float counter = 0.0;

    // Sum and decode NTSC samples
    // This is essentially a simple averaging filter
    // that happens to be weighted by two cos and sin
    // oscillators at a very specific frequency
    vec3 yiq = vec3(0.0);
    
    // Decode Luma first
    for (int d = 0; d < 51; d++) {
        vec2 p = vec2(uv.x + float(d) - 26.0, uv.y);
        vec3 s = texture(input_tex, p / PushConstants.resolution).rgb;

        float filt = lowpass(1.0/4.0, float(d), 51.0);

        yiq.x += s.x * filt;
    }

    yiq.x /= 51.0;
    
    // Then decode chroma
    for (int d = 0; d < 51; d++) {
        vec2 p = vec2(uv.x + float(d) - 26.0, uv.y);
        vec3 s = texture(input_tex, p / PushConstants.resolution).rgb;
        float t = fc * (uv.x + float(d) - 26.0) + ((PI / 2.0) * uv.y) + (PI * float((PushConstants.frame >> 1) & 1));

        // Apply Blackman window for smoother colors
        float filt = -highpass(0.25, float(d), 51.0);

        yiq.yz += s.yz * filt * vec2(cos(t), sin(t));
    }

    yiq.yz /= 51.0;
    yiq.yz *= CHROMA_SATURATION_FACTOR;

    FragColor = vec4((yiq_to_rgb * yiq), 1.0);
}