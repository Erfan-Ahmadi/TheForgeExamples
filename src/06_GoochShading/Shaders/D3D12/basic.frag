struct PointLight 
{
	float3 position;
};

StructuredBuffer <PointLight>		PointLights			: register(t0);

cbuffer LightData : register(b0)
{
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

float3 calculatePointLight(PointLight pointLight, float3 normal, float3 viewDir, float3 fragPosition, float2 TexCoord);

float4 main(VSOutput input) : SV_TARGET
{
	float3 normal = normalize(input.Normal.xyz);
	float3 viewDir = normalize(viewPos - input.FragPos.xyz);
	
	float3 result;
	
	for(int i = 0; i < numPointLights; ++i)
		result += calculatePointLight(PointLights[i], normal, viewDir, input.FragPos.xyz, input.TexCoord);

    return float4(result, 1.0f);
}

float3 calculatePointLight(PointLight pointLight, float3 normal, float3 viewDir, float3 fragPosition, float2 TexCoord)
{
	float3 colorSurface = float3(0.1f, 0.1f, 0.1f);
	float3 colorCool = float3(0, 0, 0.25) + 0.25f * colorSurface;
	float3 colorWarm = float3(0.3, 0.3, 0) + 0.25f * colorSurface;
	float3 colorHighlight = float3(0.4f, 0.4f, 0.4f);
	float3 lightDir = normalize(fragPosition - pointLight.position);

	float t = (dot(normal, lightDir) + 1) / 2.0f;
	float3 r = reflect(-lightDir, normal);
	float s = clamp(100.0f * dot(r, viewDir) - 97, 0.0f, 1.0f);

	return s * colorHighlight + (1.0f - s) * (t * colorWarm + (1.0f - t) * colorCool);
}
