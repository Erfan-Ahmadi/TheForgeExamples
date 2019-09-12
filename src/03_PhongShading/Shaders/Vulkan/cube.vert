#version 450 core

#define MAX_INSTANCES 128

layout(location = 0) in vec4 Position;
layout(location = 1) in vec4 Normal;
layout(location = 2) in vec2 TexCoords;

layout(set = 0, binding = 0) uniform UniformData
{
	uniform mat4 view;
	uniform mat4 proj;
	uniform mat4 world[MAX_INSTANCES];
};

layout(location = 0) out vec4 outNormal;
layout(location = 1) out vec4 outFragPosition;
layout(location = 2) out vec2 outTexCoords;

void main()
{
	outFragPosition = world[gl_InstanceIndex] * Position;
	outNormal = transpose(inverse(world[gl_InstanceIndex])) * Normal;
	outTexCoords = TexCoords;
	gl_Position = proj * view * outFragPosition;
}