
cbuffer LightData : register(b0)
{
	float3 lightPos;
	float3 lightColor;
};

struct VSOutput {
	float4 Position : SV_POSITION;
    float2 TexCoord : TEXCOORD;
};

SamplerState	uSampler0	: register(s2);
Texture2D		Texture		: register(t1);

float4 main(VSOutput input) : SV_TARGET
{   
	// Ambient
	float ambientStrength = 0.2f;
    float3 ambient = ambientStrength * lightColor;

	float3 color = (ambient * Texture.Sample(uSampler0, input.TexCoord).xyz);

    return float4(color, 1.0f);
}