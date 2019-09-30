cbuffer UniformData : register(b0, UPDATE_FREQ_PER_FRAME)
{
	float4x4	world;
	float4x4	view;
	float4x4	proj;
	float outlineThickness;
};

struct VSInput
{
    float4 Position : POSITION;
    float4 Normal : NORMAL;
};

struct VSOutput {
	float4 Position : SV_POSITION;
};

VSOutput main(VSInput input)
{
	VSOutput result;

	// Extuding Vertices Out
	float4 pos = float4(input.Position.xyz + input.Normal.xyz * outlineThickness, input.Position.w);
	result.Position = mul(proj, mul(view, mul(world, pos)));

	return result;
}