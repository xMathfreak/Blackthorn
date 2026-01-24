#version 330 core

in vec2 v_TexCoord;

uniform sampler2D u_Texture;
uniform vec4 u_Color;

out vec4 FragColor;

void main() {
	float alpha = texture(u_Texture, v_TexCoord).r;
	FragColor = u_Color * vec4(1.0, 1.0, 1.0, alpha);
}