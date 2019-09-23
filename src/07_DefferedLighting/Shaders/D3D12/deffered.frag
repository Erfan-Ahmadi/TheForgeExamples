
Texture2D			depthBuffer : register(t1);
Texture2D			albedo		: register(t2);
SamplerState		uSampler0	: register(s0);

struct VSOutput 
{
	float4 Position		: SV_POSITION;
    float2 TexCoord		: TEXCOORD;
};

float getDepthValue(float2 uv)
{
    float depth = depthBuffer.Sample(uSampler0, uv).x;
    return 1 - depth;
}

float4 main(VSOutput input) : SV_TARGET
{
	float depth = getDepthValue(input.TexCoord);
	return albedo.Sample(uSampler0, input.TexCoord);
    // return float4(depth, depth, depth, 1);
}