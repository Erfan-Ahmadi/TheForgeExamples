struct VSOutput 
{
	float4 Position		: SV_POSITION;
};

float4 main(VSOutput input) : SV_TARGET
{
    return float4(1, 1, 1, 1);
}