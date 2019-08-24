#define MAX_INSTANCES 128

cbuffer UniformData : register(b0)
{
	float4x4 view;
	float4x4 proj;
	float4x4 world[MAX_INSTANCES];
};

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

VSOutput main(VSInput input, uint InstanceID : SV_InstanceID)
{
	VSOutput result;
	result.Position = mul(proj, mul(view, mul(world[InstanceID], input.Position)));
	result.TexCoord = input.TexCoord;
	return result;
}