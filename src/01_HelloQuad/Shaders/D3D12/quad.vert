struct VSInput
{
    float4 Position : POSITION;
    float4 Normal : NORMAL;
    float2 TexCoord : TEXCOORD;
};

struct VSOutput {
	float4 Position : SV_POSITION;
    float2 TexCoord : TEXCOORD;
};

VSOutput main(VSInput input)
{
	VSOutput result;
	result.Position = input.Position;
	result.TexCoord = input.TexCoord;
	return result;
}