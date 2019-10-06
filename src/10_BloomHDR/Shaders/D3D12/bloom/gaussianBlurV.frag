struct VSOutput 
{
	float4 Position	: SV_POSITION;
    float2 UV		: TEXCOORD0;
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
	float2 tex_offset = 1.0f / h;
	float3 result = Texture.Sample(uSampler0, input.UV).rgb * weight[0];

	for(int i = 0; i < 5; ++i)
	{
		result += Texture.Sample(uSampler0, input.UV + float2(0.0f, tex_offset.y * i)).rgb * weight[i] * 1.5f;
		result += Texture.Sample(uSampler0, input.UV - float2(0.0f, tex_offset.y * i)).rgb * weight[i] * 1.5f;
	}
	
	return float4(result, 1.0);
}