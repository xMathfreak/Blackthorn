#version 330 core

in vec2 v_TexCoord;
in vec4 v_Color;

uniform sampler2D u_Texture;
uniform vec4 u_Color;

out vec4 FragColor;

void main() {
	float alpha = texture(u_Texture, v_TexCoord).a;
	FragColor = v_Color * u_Color * vec4(1.0, 1.0, 1.0, alpha);
}