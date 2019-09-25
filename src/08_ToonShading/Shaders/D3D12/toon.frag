struct VSOutput 
{
	float4 Position : SV_POSITION;
	float4 Normal	: NORMAL;
	float3 Color	: COLOR;
	float3 lightDir : LIGHTDIR;
};

SamplerState	uSampler0		: register(s0);

float4 main(VSOutput input) : SV_TARGET
{
	float3 color;
	float3 N = normalize(input.Normal.xyz);
	float3 L = normalize(input.lightDir);
	float intensity = dot(N,L);
	if (intensity > 0.98)
		color = input.Color * 1.5;
	else if  (intensity > 0.9)
		color = input.Color * 1.0;
	else if (intensity > 0.5)
		color = input.Color * 0.6;
	else if (intensity > 0.25)
		color = input.Color * 0.4;
	else
		color = input.Color * 0.2;
		
	float d = dot(float3(0.2126f,0.7152f,0.0722f), color);

	float3 l = lerp(color, float3(d, d, d), float3(0.1f, 0.1f, 0.1f));

	color = l;	

    return float4(color, 1.0f);
}