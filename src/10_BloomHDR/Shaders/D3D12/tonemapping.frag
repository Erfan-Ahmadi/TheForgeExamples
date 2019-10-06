#define TEX_DIM 256

struct VSOutput 
{
	float4 Position	: SV_POSITION;
    float2 UV		: TEXCOORD0;
};

SamplerState	uSampler0		: register(s0);
Texture2D		HdrTexture		: register(t0, UPDATE_FREQ_PER_FRAME);
Texture2D		BloomTexture	: register(t1, UPDATE_FREQ_PER_FRAME);

cbuffer ToneMappingData : register(b0, UPDATE_FREQ_PER_FRAME)
{
	float inExposure;
	float inGamma;
	float bloomLevel;
	bool tonemap;
};

float4 main(VSOutput input) : SV_TARGET
{    
	return BloomTexture.Sample(uSampler0, input.UV);

	// Vertical Blur
	float weight[5];
	weight[0] = 0.227027;
	weight[1] = 0.1945946;
	weight[2] = 0.1216216;
	weight[3] = 0.054054;
	weight[4] = 0.016216;
	
	float2 tex_offset = 1.0f / TEX_DIM;
	float3 hblur = BloomTexture.Sample(uSampler0, input.UV).rgb * weight[0];

	for(int i = 0; i < 5; ++i)
	{
		hblur += BloomTexture.Sample(uSampler0, input.UV + float2(tex_offset.x * i, 0.0)).rgb * weight[i] * 1.5f;
		hblur += BloomTexture.Sample(uSampler0, input.UV - float2(tex_offset.x * i, 0.0)).rgb * weight[i] * 1.5f;
	}

	const float gamma = inGamma;
  
	float4 hdrColor = HdrTexture.Sample(uSampler0, input.UV) + bloomLevel * float4(hblur, 1.0f);
	float3 mapped;

	if(tonemap)
	{
		// reinhard tone mapping
		mapped = hdrColor.xyz / (hdrColor.xyz + 1.0);
	}
	else
	{
		// Exposure tone mapping
		mapped = 1.0f - exp2(-hdrColor.xyz * inExposure);
	}
	
    // Gamma correction 
    mapped = pow(mapped, 1.0 / gamma);

    return float4(mapped, hdrColor.a);
}