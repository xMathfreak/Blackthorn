#version 330 core

layout (location = 0) in vec2 a_Position;
layout (location = 1) in vec2 a_TexCoord;
layout (location = 2) in vec4 a_Color;

layout(std140) uniform GlobalData {
	mat4 u_ViewProjection;
};

uniform vec3 u_Offset;
uniform float u_Scale;

out vec2 v_TexCoord;
out vec4 v_Color;

void main() {
	vec3 scaledPos = vec3(a_Position * u_Scale, 0) + u_Offset;
	gl_Position = u_ViewProjection * vec4(scaledPos, 1.0);
	v_TexCoord = a_TexCoord;
	v_Color = a_Color;
}