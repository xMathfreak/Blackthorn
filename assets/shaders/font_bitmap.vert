#version 330 core

layout (location = 0) in vec2 a_Position;
layout (location = 1) in vec2 a_TexCoord;

layout(std140) uniform GlobalData {
	mat4 u_ViewProjection;
};

uniform vec2 u_Offset;
uniform vec4 u_Color;

out vec2 v_TexCoord;
out vec4 v_Color;

void main() {
	vec2 translatedPos = a_Position + u_Offset;
	v_Color = u_Color;
	v_TexCoord = a_TexCoord;
	gl_Position = u_ViewProjection * vec4(translatedPos, 0.0, 1.0);
}