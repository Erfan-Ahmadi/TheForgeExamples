#version 450 core

layout(location = 0) in vec2 TexCoords;
layout(location = 1) in vec4 Normal;
layout(location = 2) in vec4 FragPos;

struct DirectionalLight 
{
    vec3 direction;
    vec3 ambient;
    vec3 diffuse;
    vec3 specular;
};

struct PointLight 
{
	vec3 position;
	vec3 ambient;
	vec3 diffuse;
	vec3 specular;
	vec3 att_params;
};

struct SpotLight 
{
	vec3 position;
	vec3 direction;
	vec2 cutOff;
	vec3 ambient;
	vec3 diffuse;
	vec3 specular;
    vec3 att_params;
};

layout (std140, set = 0, binding=4) uniform LightData
{
	uniform int numDirectionalLights;
	uniform int numPointLights;
	uniform int numSpotLights;
	uniform vec3 viewPos;
};

layout (std430, set=0, binding=5) buffer DirectionalLights
{
    DirectionalLight DirectionalLightsBuffer[];
};

layout (std430, set=0, binding=6) buffer PointLights
{
    PointLight PointLightsBuffer[];
};

layout (std430, set=0, binding=7) buffer SpotLights
{
    SpotLight SpotLightsBuffer[];
};

layout (set = 0, binding = 1) uniform texture2D		Texture;
layout (set = 0, binding = 2) uniform texture2D		TextureSpecular;
layout (set = 0, binding = 3) uniform sampler		uSampler0;

layout(location = 0) out vec4 outColor;

vec3 calculateDirectionalLight(DirectionalLight dirLight, vec3 normal, vec3 viewDir);
vec3 calculatePointLight(PointLight pointLight, vec3 normal, vec3 viewDir);
vec3 calculateSpotLight(SpotLight spotLight, vec3 normal, vec3 viewDir);

void main()
{
	vec3 normal = normalize(Normal.xyz);
	vec3 viewDir = normalize(viewPos - FragPos.xyz);
	
	vec3 result = vec3(0,0,0);

	for(int i = 0; i < numDirectionalLights; ++i)
		result += calculateDirectionalLight(DirectionalLightsBuffer[i], normal, viewDir);
	for(int i = 0; i < numPointLights; ++i)
		result += calculatePointLight(PointLightsBuffer[i], normal, viewDir);
	for(int i = 0; i < numSpotLights; ++i)
		result += calculateSpotLight(SpotLightsBuffer[i], normal, viewDir);

	outColor = vec4(result, 1);
}

vec3 calculateDirectionalLight(DirectionalLight dirLight, vec3 normal, vec3 viewDir)
{
	vec3 lightDir = normalize(-dirLight.direction);
	vec3 reflectDir = reflect(-lightDir, normal);

	// Ambient
    vec3 ambient = dirLight.ambient;

	// Diffuse
	float diff = max(dot(lightDir, normal), 0.0f);
	vec3 diffuse = diff * dirLight.diffuse;

	// Specular
	float spec = pow(max(dot(reflectDir, viewDir), 0.0), 32);
	vec3 specular = spec * dirLight.specular;  

	return (ambient + diffuse) * texture(sampler2D(Texture, uSampler0), TexCoords).xyz +
		(specular) * texture(sampler2D(TextureSpecular, uSampler0), TexCoords).xyz;
}

vec3 calculatePointLight(PointLight pointLight, vec3 normal, vec3 viewDir)
{
	vec3 difference = pointLight.position - FragPos.xyz;

	vec3 lightDir = normalize(difference);
	vec3 reflectDir = reflect(-lightDir, normal);

	// Calc Attenuation
	float distance = length(difference);
	vec3 dis = vec3(1, distance, pow(distance, 2));
	float attenuation = 1.0f / dot(dis, pointLight.att_params);

	// Ambient
    vec3 ambient = pointLight.ambient;

	// Diffuse
	float diff = max(dot(lightDir, normal), 0.0f);
	vec3 diffuse = diff * pointLight.diffuse;

	// Specular
	float spec = pow(max(dot(reflectDir, viewDir), 0.0), 64);
	vec3 specular = spec * pointLight.specular;  

	return attenuation * ((ambient + diffuse) * texture(sampler2D(Texture, uSampler0), TexCoords).xyz +
			(specular) * texture(sampler2D(TextureSpecular, uSampler0), TexCoords).xyz);
}

vec3 calculateSpotLight(SpotLight spotLight, vec3 normal, vec3 viewDir)
{
	vec3 difference = spotLight.position - FragPos.xyz;
	vec3 lightDir = normalize(difference);
	vec3 reflectDir = reflect(-lightDir, normal);
	
	float theta = dot(normalize(-spotLight.direction), lightDir);
	float epsilon = spotLight.cutOff.x - spotLight.cutOff.y;
	float intensity = clamp((theta - spotLight.cutOff.y) / epsilon , 0.0f, 1.0f);
	
	// Calc Attenuation
	float distance = length(difference);
	vec3 dis = vec3(1, distance, pow(distance, 2));
	float attenuation = 1.0f / dot(dis, spotLight.att_params);

	// Ambient
    vec3 ambient = spotLight.ambient;

	// Diffuse
	float diff = max(dot(lightDir, normal), 0.0f);
	vec3 diffuse = diff * spotLight.diffuse * intensity;

	// Specular
	float spec = pow(max(dot(reflectDir, viewDir), 0.0), 64);
	vec3 specular = spec * spotLight.specular * intensity;  

	return attenuation * ((ambient + diffuse) * texture(sampler2D(Texture, uSampler0), TexCoords).xyz +
			(specular) * texture(sampler2D(TextureSpecular, uSampler0), TexCoords).xyz);
}
