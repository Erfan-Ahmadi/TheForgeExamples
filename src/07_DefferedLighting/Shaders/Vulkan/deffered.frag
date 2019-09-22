#version 450 core

layout(location = 0) out vec4 outColor;

layout(set = 0, binding = 1) uniform texture2D depthBuffer;

void main()
{
	outColor = vec4(1, 1, 1, 1);
}