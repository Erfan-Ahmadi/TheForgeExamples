struct VSInput
{
    float4 Position : POSITION;
};

struct VSOutput 
{
	float4 Position		: SV_POSITION;
};

VSOutput main(VSInput input)
{
	VSOutput result;
	result.Position = mul(proj, mul(view,  mul(world, input.Position)));
	return result;
}