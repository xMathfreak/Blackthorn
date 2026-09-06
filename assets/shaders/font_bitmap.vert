#version 330 core

layout (location = 0) in vec2 a_Position;
layout (location = 1) in vec2 a_TexCoord;
layout (location = 2) in vec4 a_Color;

layout(std140) uniform GlobalData {
	mat4 u_ViewProjection;
};

uniform vec3 u_Offset;
out vec2 v_TexCoord;
out vec4 v_Color;

void main() {
	vec3 translatedPos = vec3(a_Position, 0.0) + u_Offset;
	v_TexCoord = a_TexCoord;
	v_Color = a_Color;
	gl_Position = u_ViewProjection * vec4(translatedPos, 1.0);
}