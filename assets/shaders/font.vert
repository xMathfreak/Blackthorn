#version 330 core

layout(location = 0) in vec3 a_Position;
layout(location = 1) in vec4 a_Color;
layout(location = 2) in vec2 a_TexCoord;

layout(std140) uniform GlobalData {
	mat4 u_ViewProjection;
};

uniform vec2 u_Offset;

out vec4 v_Color;
out vec2 v_TexCoord;

void main() {
	vec3 translatedPos = a_Position + vec3(u_Offset, 0.0);
	v_Color = a_Color;
	v_TexCoord = a_TexCoord;
	gl_Position = u_ViewProjection * vec4(translatedPos, 1.0);
}