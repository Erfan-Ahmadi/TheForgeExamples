struct DirectionalLight {
    float3 direction;
    float3 ambient;
    float3 diffuse;
    float3 specular;
};

StructuredBuffer <DirectionalLight> DirectionalLights : register(t0);

cbuffer LightData : register(b0)
{
	int numDirectionalLights;
	float3 viewPos;
};

struct VSOutput {
	float4 Normal		: NORMAL;
	float4 FragPos		: POSITION;
	float4 Position		: SV_POSITION;
    float2 TexCoord		: TEXCOORD;
};

SamplerState	uSampler0		: register(s1);
Texture2D		Texture			: register(t2);
Texture2D		TextureSpecular	: register(t3);

float3 calculateDirectionalLight(DirectionalLight dirLight, float3 normal, float3 viewDir, float2 TexCoord);

float4 main(VSOutput input) : SV_TARGET
{
	float3 normal = normalize(input.Normal.xyz);
	float3 viewDir = normalize(viewPos - input.FragPos.xyz);
	
	float3 result;

	for(int i = 0; i < numDirectionalLights; ++i)
		result += calculateDirectionalLight(DirectionalLights[i], normal, viewDir, input.TexCoord);

    return float4(result, 1.0f);
}

float3 calculateDirectionalLight(DirectionalLight dirLight, float3 normal, float3 viewDir, float2 TexCoord)
{
	float3 lightDir = normalize(-dirLight.direction);
	float3 reflectDir = reflect(-lightDir, normal);

	// Ambient
    float3 ambient = dirLight.ambient;

	// Diffuse
	float diff = max(dot(lightDir, normal), 0.0f);
	float3 diffuse = diff * dirLight.diffuse;

	// Specular
	float spec = pow(max(dot(reflectDir, viewDir), 0.0), 64);
	float3 specular = spec * dirLight.specular;  

	return (ambient + diffuse) * Texture.Sample(uSampler0, TexCoord).xyz +
		(specular) * Texture.Sample(uSampler0, TexCoord).xyz;
}
