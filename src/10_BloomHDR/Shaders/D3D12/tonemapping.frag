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
	float3 bloom = BloomTexture.Sample(uSampler0, input.UV).rgb;
	//return float4(bloom, 1.0f);

	const float gamma = inGamma;
  
	float4 hdrColor = HdrTexture.Sample(uSampler0, input.UV) + bloomLevel * float4(bloom, 1.0f);
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