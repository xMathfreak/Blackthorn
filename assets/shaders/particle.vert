#version 330 core

layout (location = 0) in vec2 a_Position;
layout (location = 1) in vec2 a_UV;

layout (location = 2) in vec2 i_Position;
layout (location = 3) in vec4 i_Color;
layout (location = 4) in float i_Size;
layout (location = 5) in float i_Rotation;
layout (location = 6) in vec4 i_UVRect;

layout(std140) uniform GlobalData {
	mat4 u_ViewProjection;
	float u_RenderTime;
	float u_FixedTime;
};

out vec2 v_UV;
out vec4 v_Color;

void main() {
	float c = cos(i_Rotation);
	float s = sin(i_Rotation);

	mat2 rotation = mat2(
		c, -s,
		s, c
	);

	vec2 worldPosition = i_Position + rotation * (a_Position * i_Size);
	gl_Position = u_ViewProjection * vec4(worldPosition, 0.0, 1.0);

	v_UV = i_UVRect.xy + a_UV * i_UVRect.zw;
	v_Color = i_Color;
}