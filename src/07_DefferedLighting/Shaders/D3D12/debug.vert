
cbuffer uniformData : register(b0, UPDATE_FREQ_PER_FRAME)
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
	result.Position = mul(proj, input.Position);
	result.Position.z = input.Position.z;
	result.TexCoord = input.TexCoord;
	return result;
}