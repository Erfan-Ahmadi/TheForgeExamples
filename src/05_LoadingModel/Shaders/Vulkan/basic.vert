#version 450 core

#define MAX_INSTANCES 8

layout(location = 0) in vec4 Position;
layout(location = 1) in vec4 Normal;
layout(location = 2) in vec2 TexCoords;

layout(set = 0, binding = 0) uniform UniformData
{
	uniform mat4 view;
	uniform mat4 proj;
	uniform mat4 world;
};

layout(location = 0) out vec2 outTexCoords;
layout(location = 1) out vec4 outNormal;
layout(location = 2) out vec4 outFragPos;

void main()
{
	outFragPos = world * Position;
	gl_Position = proj * view * outFragPos;
	outTexCoords = TexCoords;
	outNormal = transpose(inverse(world)) * Normal;
}