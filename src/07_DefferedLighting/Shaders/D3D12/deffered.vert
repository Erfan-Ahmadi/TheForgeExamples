
cbuffer uniformData : register(b0)
{
	float4x4 proj;
};

struct VSInput
{
    float4 Position : POSITION;
    float2 TexCoord : TEXCOORD;
};

struct VSOutput {
	float4 Position : SV_POSITION;
    float2 TexCoord : TEXCOORD;
};

VSOutput main(VSInput input, uint VertexID: SV_VertexID)
{
	VSOutput result;
	result.Position = input.Position;
	result.TexCoord = input.TexCoord;
	return result;
}