
Texture2D<float>	depthBuffer : register(t1);

struct VSOutput 
{
	float4 Position		: SV_POSITION;
    float2 TexCoord		: TEXCOORD;
};

float getDepthValue()
{
	return 0.5f;
}

float4 main(VSOutput input) : SV_TARGET
{
	float depth = getDepthValue();
    return float4(depth, depth, depth, 1);
}