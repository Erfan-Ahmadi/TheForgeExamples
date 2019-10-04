cbuffer UniformData : register(b0, UPDATE_FREQ_PER_FRAME)
{
	float4x4 view;
	float4x4 proj;
};

struct VSInput
{
    float3 Position			: POSITION;
    float3 InstancePosition : TEXCOORD0;
    float3 InstanceColor	: COLOR;
};

struct VSOutput 
{
	float4 Position		: SV_POSITION;
};

VSOutput main(VSInput input)
{
	VSOutput result;
	float4 worldPos = float4(input.InstancePosition + input.Position, 0.0f); 
	result.Position = mul(proj, mul(view,  worldPos));
	return result;
}