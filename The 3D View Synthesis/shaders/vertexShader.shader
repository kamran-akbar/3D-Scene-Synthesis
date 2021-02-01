#version 430 core
layout (location = 0) in vec4 pos;
layout (location = 1) in vec4 color;

out vec4 vertexColor;

uniform mat4 mvp;

void main()
{
	gl_Position = mvp * pos;
	vertexColor = color;
}