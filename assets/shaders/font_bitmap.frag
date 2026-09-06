#version 330 core

in vec2 v_TexCoord;
in vec4 v_Color;

out vec4 FragColor;

uniform sampler2D u_Texture;
uniform vec4 u_Color;

void main() {
	vec4 texColor = texture(u_Texture, v_TexCoord);

	if (texColor.a < 0.1)
		discard;

	FragColor = texColor * u_Color * v_Color;
}