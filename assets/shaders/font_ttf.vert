#version 330 core

layout (location = 0) in vec2 a_Position;
layout (location = 1) in vec2 a_TexCoord;

layout(std140) uniform GlobalData {
	mat4 u_ViewProjection;
};

uniform vec4 u_Color;
uniform vec2 u_Position;
uniform float u_Scale;

out vec2 v_TexCoord;
out vec4 v_Color;

void main() {
	vec2 scaledPos = a_Position * u_Scale + u_Position;
	gl_Position = u_ViewProjection * vec4(scaledPos, 0.0, 1.0);
	v_TexCoord = a_TexCoord;
	v_Color = u_Color;
}