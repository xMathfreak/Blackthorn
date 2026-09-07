#version 330 core

in vec2 v_TexCoord;
in vec4 v_Color;
in float v_ShadowBlur;

out vec4 FragColor;

uniform sampler2D u_Texture;
uniform vec4 u_Color;
uniform bool u_ShadowPass;

void main() {
	if (u_ShadowPass) {
		float alpha = texture(u_Texture, v_TexCoord).a;

		if (v_ShadowBlur > 0.0) {
			vec2 texel = 1.0 / vec2(textureSize(u_Texture, 0));
			float accum = 0.0;

			for (int x = -2; x <= 2; ++x)
				for (int y = -2; y <= 2; ++y)
					accum += texture(u_Texture, v_TexCoord + vec2(x, y) * texel * v_ShadowBlur).a;

			alpha = accum / 25.0;
		}

		if (alpha < 0.01)
			discard;

		FragColor = vec4(v_Color.rgb, v_Color.a * alpha);
	} else {
		vec4 texColor = texture(u_Texture, v_TexCoord);

		if (texColor.a < 0.1)
			discard;

		FragColor = texColor * u_Color * v_Color;
	}
}