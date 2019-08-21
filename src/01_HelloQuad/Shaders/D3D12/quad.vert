struct VSInput
{
    float4 Position : POSITION;
    float4 Normal : NORMAL;
};

struct VSOutput {
	float4 Position : SV_POSITION;
    float4 Color : COLOR;
};

VSOutput main(VSInput input)
{
	VSOutput result;
	result.Position = input.Position;
	result.Color = float4(1.0f, 1.0f, 1.0f, 1.0f);
}