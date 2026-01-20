#version 330 core

layout (location = 0) in vec2 a_Pos;
layout (location = 1) in vec4 a_Color;
layout (location = 2) in vec2 a_TexCoord;

uniform mat4 u_Projection;
uniform vec2 u_Position;
uniform float u_Scale;

out vec2 v_TexCoord;
out vec4 v_Color;

void main() {
	vec2 scaledPos = a_Pos * u_Scale + u_Position;
	gl_Position = u_Projection * vec4(scaledPos, 0.0, 1.0);
	v_TexCoord = a_TexCoord;
	v_Color = a_Color;
}