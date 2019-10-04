
struct VSOutput 
{
	float4 Position	: SV_POSITION;
    float2 UV		: TEXCOORD0;
};

SamplerState	uSampler0	: register(s0);
Texture2D		HdrTexture	: register(t0, UPDATE_FREQ_PER_FRAME);

float4 main(VSOutput input) : SV_TARGET
{
    return float4(HdrTexture.Sample(uSampler0, input.UV).xyz, 1.0f);
}