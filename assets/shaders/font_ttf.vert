#version 330 core

layout (location = 0) in vec2 a_Position;
layout (location = 1) in vec2 a_TexCoord;
layout (location = 2) in vec4 a_Color;

layout (location = 3) in vec2 i_Position;
layout (location = 4) in vec2 i_Size;
layout (location = 5) in vec4 i_UV;
layout (location = 6) in vec4 i_Color;
layout (location = 7) in float i_Z;
layout (location = 8) in float i_Rotation;
layout (location = 9) in vec2 i_ShadowOffset;
layout (location = 10) in vec4 i_ShadowColor;
layout (location = 11) in float i_ShadowBlur;
layout (location = 12) in vec2 i_Shake;


layout(std140) uniform GlobalData {
	mat4 u_ViewProjection;
	float u_RenderTime;
	float u_FixedTime;
};

uniform vec3 u_Offset;
uniform float u_Scale;
uniform bool u_DynamicMode;
uniform bool u_ShadowPass;

const vec2 QUAD[4] = vec2[](
	vec2(0.0, 0.0), vec2(1.0, 0.0),
	vec2(0.0, 1.0), vec2(1.0, 1.0)
);

out vec2 v_TexCoord;
out vec4 v_Color;
out float v_ShadowBlur;

void main() {
	vec2 localPos;
	vec2 texCoord;
	vec4 color;
	vec3 worldPos;
	float z;

	if (u_DynamicMode) {
		int vid = gl_VertexID % 4;
		localPos = QUAD[vid] * i_Size;
		texCoord = vec2(mix(i_UV.x, i_UV.z, QUAD[vid].x), mix(i_UV.w, i_UV.y, QUAD[vid].y));
		color = i_Color;
		z = i_Z;

		float c = cos(i_Rotation);
		float s = sin(i_Rotation);

		mat2 rot = mat2(c, -s, s, c);
		localPos = rot * localPos;

		vec2 shakeOffset = vec2(0.0);
		if (i_Shake.x > 0.0) {
			float phase = float(gl_InstanceID) * 2.399963;
			shakeOffset = vec2(
				sin(u_RenderTime * i_Shake.y + phase),
				cos(u_RenderTime * i_Shake.y * 1.3 + phase * 1.7)
			) * i_Shake.x;
		}

		vec2 shadowOffset = u_ShadowPass ? i_ShadowOffset : vec2(0.0);
		worldPos = vec3((i_Position + localPos + shadowOffset + shakeOffset) * u_Scale, z) + u_Offset;
	} else {
		localPos = a_Position;
		texCoord = a_TexCoord;
		color = a_Color;
		z = 0.0;
		worldPos = vec3(a_Position * u_Scale, 0.0) + u_Offset;
	}

	v_TexCoord = texCoord;
	v_Color = u_ShadowPass ? (u_DynamicMode ? i_ShadowColor : vec4(0.0)) : color;
	v_ShadowBlur = u_DynamicMode ? i_ShadowBlur : 0.0;
	gl_Position = u_ViewProjection * vec4(worldPos, 1.0);
}