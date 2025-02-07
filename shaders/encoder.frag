// Encoder or Modulator
// This pass converts RGB colors on iChannel0 to
// a YIQ (NTSC) Composite signal.

#define PI   3.14159265358979323846
#define TAU  6.28318530717958647693

const mat3 rgb_to_yiq = mat3(
    0.299, 0.596, 0.211,
    0.587,-0.274,-0.523,
    0.114,-0.322, 0.312
);

void main() {
    vec2 uv = FragTexCoord * screen_size;

    // Chroma encoder oscillator frequency 
    float fc = 1000.0f;

    // Base oscillator angle for this dot
    float t = uv.x;
    
    // Get a pixel from input_texture
    vec3 rgb = texture(input_texture, uv / screen_size).rgb;

    // Convert to YIQ
    vec3 yiq = rgb_to_yiq * rgb;
    
    // Final oscillator angle
    float f = fc * t + ((PI / 2.0) * uv.y) + ((PI) * float(frame & 1));
    
    // Modulate IQ signals
    float i = yiq.y * cos(f), // I signal
          q = yiq.z * sin(f); // Q signal
    
    // Add to Y to get the composite signal
    float c = yiq.x + (i + q); // Composite
    
    // Return a grayscale representation of the signal
    FragColor = vec4(vec3(c), 1.0);
}