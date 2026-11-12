#version 330 core

layout(location = 0) out vec4 FragColor;

in vec4 v_Color;
in vec2 v_TexCoord;
in float v_TexIndex;

uniform sampler2D u_Textures[16];

vec4 SampleTexture(int index, vec2 texCoords)
{
	switch (index)
	{
		case 0:  return texture(u_Textures[0], texCoords);
		case 1:  return texture(u_Textures[1], texCoords);
		case 2:  return texture(u_Textures[2], texCoords);
		case 3:  return texture(u_Textures[3], texCoords);
		case 4:  return texture(u_Textures[4], texCoords);
		case 5:  return texture(u_Textures[5], texCoords);
		case 6:  return texture(u_Textures[6], texCoords);
		case 7:  return texture(u_Textures[7], texCoords);
		case 8:  return texture(u_Textures[8], texCoords);
		case 9:  return texture(u_Textures[9], texCoords);
		case 10: return texture(u_Textures[10], texCoords);
		case 11: return texture(u_Textures[11], texCoords);
		case 12: return texture(u_Textures[12], texCoords);
		case 13: return texture(u_Textures[13], texCoords);
		case 14: return texture(u_Textures[14], texCoords);
		case 15: return texture(u_Textures[15], texCoords);

		default: return vec4(1.0);
	}
}

void main() {
	int index = int(v_TexIndex);
	vec4 texColor = SampleTexture(index, v_TexCoord);

	if (texColor.a < 0.1)
		discard;

	FragColor = texColor * v_Color;
}