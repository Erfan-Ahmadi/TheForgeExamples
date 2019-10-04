
struct VSOutput 
{
	float4 Position	: SV_POSITION;
    float2 UV		: TEXCOORD0;
};

SamplerState	uSampler0	: register(s0);
Texture2D		HdrTexture	: register(t0, UPDATE_FREQ_PER_FRAME);

cbuffer ToneMappingData : register(b0, UPDATE_FREQ_PER_FRAME)
{
	float inExposure;
	float inGamma;
	bool tonemap;
};

float4 main(VSOutput input) : SV_TARGET
{    
	const float gamma = inGamma;
  
	float3 hdrColor = HdrTexture.Sample(uSampler0, input.UV).xyz;
	float3 mapped;

	if(tonemap)
	{
		// reinhard tone mapping
		mapped = hdrColor / (hdrColor + 1.0);
	}
	else
	{
		// Exposure tone mapping
		mapped = 1.0f - exp2(-hdrColor * inExposure);
	}
	
    // Gamma correction 
    mapped = pow(mapped, 1.0 / gamma);

    return float4(mapped, 1.0f);
}