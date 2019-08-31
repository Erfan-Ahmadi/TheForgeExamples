#version 450 core

layout(location = 0) in vec2 TexCoords;
layout(location = 1) in vec4 Normal;
layout(location = 2) in vec4 FragPos;

struct PointLight 
{
	vec3 position;
};

layout (std140, set = 0, binding=4) uniform LightData
{
	uniform int numPointLights;
	uniform vec3 viewPos;
};

layout (std430, set=0, binding=5) buffer PointLights
{
    PointLight PointLightsBuffer[];
};


layout (set = 0, binding = 1) uniform texture2D		Texture;
layout (set = 0, binding = 2) uniform texture2D		TextureSpecular;
layout (set = 0, binding = 3) uniform sampler		uSampler0;

layout(location = 0) out vec4 outColor;

vec3 calculatePointLight(PointLight pointLight, vec3 normal, vec3 viewDir);

void main()
{
	vec3 normal = normalize(Normal.xyz);
	vec3 viewDir = normalize(viewPos - FragPos.xyz);
	
	vec3 result = vec3(0,0,0);

	for(int i = 0; i < numPointLights; ++i)
		result += calculatePointLight(PointLightsBuffer[i], normal, viewDir);

	outColor = vec4(result, 1);
}

vec3 calculatePointLight(PointLight pointLight, vec3 normal, vec3 viewDir)
{
	vec3 colorSurface = vec3(0.1f, 0.1f, 0.1f);
	vec3 colorCool = vec3(0, 0, 0.25) + 0.25f * colorSurface;
	vec3 colorWarm = vec3(0.3, 0.3, 0) + 0.25f * colorSurface;
	vec3 colorHighlight = vec3(0.4f, 0.4f, 0.4f);
	vec3 lightDir = normalize(FragPos.xyz - pointLight.position);

	float t = (dot(normal, lightDir) + 1) / 2.0f;
	vec3 r = reflect(-lightDir, normal);
	float s = clamp(100.0f * dot(r, viewDir) - 97, 0.0f, 1.0f);

	return s * colorHighlight + (1.0f - s) * (t * colorWarm + (1.0f - t) * colorCool);
}