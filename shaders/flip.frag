void main() {
    vec3 rgb = texture(input_texture, vec2(FragTexCoord.x, 1.0 - FragTexCoord.y)).xyz;

    FragColor = vec4(rgb, 1.0);
}