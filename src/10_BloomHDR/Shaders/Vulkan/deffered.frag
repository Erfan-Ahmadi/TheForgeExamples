#version 450 core

layout(location = 0) in vec2 TexCoords;

layout(location = 0) out vec4 outColor;

layout(set = 0, binding = 0) uniform texture2D depthBuffer;
layout(set = 0, binding = 1) uniform sampler   uSampler0;

float getDepthValue()
{
    float depth = texture(sampler2D(depthBuffer, uSampler0), TexCoords).x;
	return 1 - depth;
}

void main()
{
	float depth = getDepthValue();
	outColor = vec4(depth, depth, depth, 1);
}