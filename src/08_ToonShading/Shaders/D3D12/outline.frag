struct VSOutput 
{
	float4 Position : SV_POSITION;
};

float4 main(VSOutput input) : SV_TARGET
{
    return float4(float3(0.0f, 1.0f, 0.0f), 1.0f);
}