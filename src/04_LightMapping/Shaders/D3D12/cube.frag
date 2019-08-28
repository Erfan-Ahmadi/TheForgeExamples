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

StructuredBuffer <DirectionalLight> DirectionalLights	: register(t0);
StructuredBuffer <PointLight>		PointLights			: register(t1);

cbuffer LightData : register(b0)
{
	int numDirectionalLights;
	int numPointLights;
	float3 viewPos;
};

struct VSOutput 
{
	float4 Normal		: NORMAL;
	float4 FragPos		: POSITION;
	float4 Position		: SV_POSITION;
    float2 TexCoord		: TEXCOORD;
};

SamplerState	uSampler0		: register(s0);
Texture2D		Texture			: register(t5);
Texture2D		TextureSpecular	: register(t6);

float3 calculateDirectionalLight(DirectionalLight dirLight, float3 normal, float3 viewDir, float2 TexCoord);
float3 calculatePointLight(PointLight pointLight, float3 normal, float3 viewDir, float3 fragPosition, float2 TexCoord);

float4 main(VSOutput input) : SV_TARGET
{
	float3 normal = normalize(input.Normal.xyz);
	float3 viewDir = normalize(viewPos - input.FragPos.xyz);
	
	float3 result;
	
	if(PointLights[0].position.y == 0.0f)
	{
		return float4(1,1,0,1);
	}
	
	if(DirectionalLights[0].direction.y == 0.0f)
	{
		return float4(0,1,1,1);
	}

    return float4(0, 1, 0, 1.0f);

	for(int i = 0; i < numDirectionalLights; ++i)
		result += calculateDirectionalLight(DirectionalLights[i], normal, viewDir, input.TexCoord);
	for(int i = 0; i < numPointLights; ++i)
		result += calculatePointLight(PointLights[i], normal, viewDir, input.FragPos.xyz, input.TexCoord);

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
	float spec = pow(max(dot(reflectDir, viewDir), 0.0), 32);
	float3 specular = spec * dirLight.specular;  

	return (ambient + diffuse) * Texture.Sample(uSampler0, TexCoord).xyz +
		(specular) * TextureSpecular.Sample(uSampler0, TexCoord).xyz;
}

float3 calculatePointLight(PointLight pointLight, float3 normal, float3 viewDir, float3 fragPosition, float2 TexCoord)
{
	float3 difference = pointLight.position - fragPosition;

	float3 lightDir = normalize(difference);
	float3 reflectDir = reflect(-lightDir, normal);

	// Calc Attenuation
	float distance = length(difference);
	float3 dis = float3(1, distance, pow(distance, 2));
	float3 att = float3(pointLight.att_constant, pointLight.att_linear, pointLight.att_quadratic);
	float attenuation = 1.0f / dot(dis, att);

	// Ambient
    float3 ambient = pointLight.ambient;

	// Diffuse
	float diff = max(dot(lightDir, normal), 0.0f);
	float3 diffuse = diff * pointLight.diffuse;

	// Specular
	float spec = pow(max(dot(reflectDir, viewDir), 0.0), 64);
	float3 specular = spec * pointLight.specular;  

	return attenuation * ((ambient + diffuse) * Texture.Sample(uSampler0, TexCoord).xyz +
			(specular) * TextureSpecular.Sample(uSampler0, TexCoord).xyz);
}
