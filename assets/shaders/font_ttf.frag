#version 330 core

in vec2 v_TexCoord;

uniform sampler2D u_Texture;
uniform vec4 u_Color;

out vec4 FragColor;

void main() {
	float dist = texture(u_Texture, v_TexCoord).r;

	float smoothing = fwidth(dist) * 0.5;
	float alpha = smoothstep(0.5 - smoothing, 0.5 + smoothing, dist);

	FragColor = vec4(u_Color.rgb, u_Color.a * alpha);
}