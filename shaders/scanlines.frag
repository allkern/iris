#version 460

layout (location = 0) in vec2 TexCoord;
layout (location = 0) out vec4 FragColor;
layout (binding = 0) uniform sampler2D input_tex;

layout(push_constant) uniform constants {
    vec2 resolution;
    int frame;
} PushConstants;

// NTSC-like shader
// Simulates scanlines, horizontal blur, and basic color bleeding

void main()
{ 
    vec2 uv = TexCoord;
    vec2 oneX = vec2(1.0 / PushConstants.resolution.x, 0.0);
    
    // YIQ / RGB Matrices
    // RGB to YIQ
    const vec3 kY = vec3(0.299, 0.587, 0.114);
    const vec3 kI = vec3(0.596, -0.274, -0.322);
    const vec3 kQ = vec3(0.211, -0.523, 0.312);

    // 1. & 3. Bandwidth extraction (Luma) & Bleed (Chroma)
    // Sample a window for Chroma (Low Bandwidth)
    // Sample center for Luma (High Bandwidth)
    
	vec3 centerRGB = texture(input_tex, uv).rgb;
    float y = dot(centerRGB, kY);
    
    float iSum = 0.0;
    float qSum = 0.0;
    float weightSum = 0.0;
    
    // NTSC color carrier is ~3.58MHz, bandwidth allows for ~1/3 PushConstants.resolution of luma.
    // We smear chroma over a few pixels.
    float bleed = 2.0; 
    for(float x = -bleed; x <= bleed; x += 1.0)
    {
        vec3 rgb = texture(input_tex, uv + oneX * x).rgb;
        float i = dot(rgb, kI);
        float q = dot(rgb, kQ);
        
        // Gaussianish weight
        float w = 1.0 / (1.0 + abs(x));
        
        iSum += i * w;
        qSum += q * w;
        weightSum += w;
    }
    
    float finalI = iSum / weightSum;
    float finalQ = qSum / weightSum;
    
    // Convert back to RGB
    // R = Y + 0.956I + 0.621Q
    // G = Y - 0.272I - 0.647Q
    // B = Y - 1.106I + 1.703Q
    
    vec3 color = vec3(
        y + 0.956 * finalI + 0.621 * finalQ,
        y - 0.272 * finalI - 0.647 * finalQ,
        y - 1.106 * finalI + 1.703 * finalQ
    );

    // 2. Scanlines
    // Based on UV.y. 240 lines.
    float scanline = sin(uv.y * 240.0 * 3.14159 * 2.0);
    color *= (0.95 + 0.05 * scanline);
    
    // 4. Gamma
    // Input is usually linear or sRGB. Output to sRGB.
    // Assuming input acts as sRGB for now.
    
    FragColor = vec4(color, 1.0);
}
