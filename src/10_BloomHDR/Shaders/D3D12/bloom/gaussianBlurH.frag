#define TEX_DIM 256

struct VSOutput 
{
	float4 Position	: SV_POSITION;
    float2 UV		: TEXCOORD0;
};

cbuffer BlurData : register(b0, UPDATE_FREQ_PER_FRAME)
{
	float strength;
};

SamplerState	uSampler0	: register(s0);
Texture2D		Texture		: register(t0, UPDATE_FREQ_PER_FRAME);

float4 main(VSOutput input) : SV_TARGET
{    
	// Horizontal Blurr
	// * Vertical blur in Composition Pass
	float weight[5];
	weight[0] = 0.227027;
	weight[1] = 0.1945946;
	weight[2] = 0.1216216;
	weight[3] = 0.054054;
	weight[4] = 0.016216;
	
	uint w, h;
	Texture.GetDimensions(w, h);
	float2 tex_offset = 1.0f / w;
	float3 result = Texture.Sample(uSampler0, input.UV).rgb * weight[0];

	for(int i = 0; i < 5; ++i)
	{
		result += Texture.Sample(uSampler0, input.UV + float2(tex_offset.x * i, 0.0f)).rgb * weight[i] * strength;
		result += Texture.Sample(uSampler0, input.UV - float2(tex_offset.x * i, 0.0f)).rgb * weight[i] * strength;
	}
	
	return float4(result, 1.0);
}