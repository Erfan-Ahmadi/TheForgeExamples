#version 450 core

layout(location = 0) in vec4 inNormal;
layout(location = 1) in vec4 inFragPosition;
layout(location = 2) in vec2 inTexCoords;

layout (set = 0, binding = 1) uniform texture2D		Texture;
layout (set = 0, binding = 2) uniform sampler		uSampler0;

layout (std140, set = 0, binding=3) uniform LightData
{
	vec3 lightPos;
	vec3 lightColor;
	vec3 viewPos;
};

layout(location = 0) out vec4 outColor;

void main()
{
	vec3 normal = normalize(inNormal.xyz);
	vec3 lightDir = normalize(lightPos - inFragPosition.xyz);
	
	vec3 viewDir = normalize(viewPos - inFragPosition.xyz);
	vec3 reflectDir = reflect(-lightDir, normal);

	// Ambient
	float ambientStrength = 0.2f;
    vec3 ambient = ambientStrength * lightColor;

	// Diffuse
	float diff = max(dot(lightDir, normal), 0.0f);
	vec3 diffuse = diff * lightColor;

	// Specular
	float specularStrenght = 1.5f;
	float spec = pow(max(dot(reflectDir, viewDir), 0.0), 64);
	vec3 specular = specularStrenght * spec * lightColor;  

	vec3 color = (ambient + diffuse + specular) * texture(sampler2D(Texture, uSampler0), inTexCoords).xyz;

    outColor = vec4(color, 1.0f);
	return;
}