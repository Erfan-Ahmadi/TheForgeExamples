
cbuffer LightData : register(b0)
{
	float4 lightPos;
	float4 lightColor;
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
	float ambientStrength = 1.0;
    float4 ambient = float4(1,1,1,1);

    return lightColor * Texture.Sample(uSampler0, input.TexCoord);
}