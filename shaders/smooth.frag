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

void main() {
    vec2 uv = FragTexCoord; // curvature(FragTexCoord, 5.5);
    vec3 rgb = vec3(0.0);

	if (uv.x < 0.0 || uv.x > 1.0 || uv.y < 0.0 || uv.y > 1.0)
		discard;

    uv *= screen_size;

    for (int y = -1; y < 2; y++) {
        for (int x = 0; x < 2; x++) {
            vec2 p = vec2(uv.x + float(x), uv.y + float(y));

            rgb += texture(input_texture, p / screen_size).xyz;
        }
    }

    rgb /= 6.0;

    FragColor = vec4(rgb, 1.0);
}