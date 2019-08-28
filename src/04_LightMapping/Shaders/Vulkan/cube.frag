#version 450 core

layout(location = 0) in vec2 TexCoords;
layout(location = 1) in vec4 Normal;
layout(location = 2) in vec4 FragPos;

struct DirectionalLight 
{
    float3 direction;
    float3 ambient;
    float3 diffuse;
    float3 specular;
};

struct PointLight 
{
	float3 position;
	float3 ambient;
	float3 diffuse;
	float3 specular;
	
    float att_constant;
    float att_linear;
    float att_quadratic;
	float _pad0;
};

layout (set=0, binding=3) buffer DirectionalLights
{
    DirectionalLight DirectionalLightsBuffer[];
};

layout (set=0, binding=4) buffer PointLights
{
    PointLight PointLightsBuffer[];
};

layout (set = 0, binding = 1) uniform texture2D		Texture;
layout (set = 0, binding = 2) uniform sampler		uSampler0;

layout(location = 0) out vec4 outColor;

void main()
{
	outColor = texture(sampler2D(Texture, uSampler0), TexCoords);
}