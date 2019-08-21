#version 450 core

layout(location = 0) in vec4 Position;
layout(location = 1) in vec4 Normal;

layout(location = 0) out vec4 Color;

void main()
{
	gl_Position = Position;
	Color = vec4(1.0f, 1.0f, 1.0f, 1.0f);
}