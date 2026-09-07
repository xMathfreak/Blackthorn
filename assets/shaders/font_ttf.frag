#version 330 core

in vec2 v_TexCoord;
in vec4 v_Color;
in float v_ShadowBlur;

uniform sampler2D u_Texture;
uniform vec4 u_Color;
uniform bool u_ShadowPass;

out vec4 FragColor;

float sampleSDF(vec2 uv) {
	return texture(u_Texture, uv).r;
}

void main() {
	float dist = sampleSDF(v_TexCoord);
	float smoothing = fwidth(dist) * 0.5;
	float alpha = smoothstep(0.5 - smoothing, 0.5 + smoothing, dist);

	if (u_ShadowPass && v_ShadowBlur > 0.0) {
		vec2 texel = 1.0 / vec2(textureSize(u_Texture, 0));
		float accum = 0.0;

		for (int x = -2; x <= 2; ++x)
			for (int y = -2; y <= 2; ++y)
			accum += sampleSDF(v_TexCoord + vec2(x, y) * texel * v_ShadowBlur);

		accum /= 25.0;
		float shadowAlpha = smoothstep(0.5 - smoothing, 0.5 + smoothing, dist);
		FragColor = vec4(v_Color.rgb, v_Color.a * shadowAlpha);
	} else {
		if (alpha < 0.01)
			discard;

		FragColor = vec4((u_Color * v_Color).rgb, u_Color.a * v_Color.a * alpha);
	}
}