#version 450 core

layout(location = 0) in vec4 Position;

layout(set = 0, binding = 0) uniform UniformData
{
	uniform mat4 view;
	uniform mat4 proj;
	uniform mat4 world;
};

void main()
{
	gl_Position = proj * view * world * Position;
}