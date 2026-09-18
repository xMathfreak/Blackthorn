#version 330 core

in vec2 v_UV;
in vec4 v_Color;

layout (location = 0) out vec4 FragColor;

uniform sampler2D u_Texture;

void main() {
	vec4 texColor = texture(u_Texture, v_UV);

	if (texColor.a < 0.1)
		discard;

	FragColor = texColor * v_Color;
}