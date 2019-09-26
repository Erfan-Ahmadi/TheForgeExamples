struct VSOutput 
{
	float4 Position : SV_POSITION;
	float3 Normal	: NORMAL;
};

SamplerState	uSampler0		: register(s0);

float4 main(VSOutput input) : SV_TARGET
{
    return float4(0, 0, 0, 1.0f);
}