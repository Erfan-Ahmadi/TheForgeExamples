
Texture2D			depthBuffer		: register(t1);
Texture2D			albedoSpec		: register(t2);
Texture2D			normal			: register(t3);
Texture2D			position		: register(t4);
SamplerState		uSampler0	: register(s0);

struct VSOutput 
{
	float4 Position		: SV_POSITION;
    float2 TexCoord		: TEXCOORD;
};

float getDepthValue(float2 uv)
{
    float depth = depthBuffer.Sample(uSampler0, uv).x;
    return 1 - depth;
}

float4 main(VSOutput input) : SV_TARGET
{
	float depth = getDepthValue(input.TexCoord);

    float3 Albedo = albedoSpec.Sample(uSampler0, input.TexCoord).rgb;
    float3 Normal = normal.Sample(uSampler0, input.TexCoord).rgb;
    float3 Position = position.Sample(uSampler0, input.TexCoord).rgb;
    float Specular = albedoSpec.Sample(uSampler0, input.TexCoord).a;

	float3 result;
	
	if(input.Position.z == 0.1f)
		return float4(Albedo, 1.0f);
	if(input.Position.z == 0.2f)
		return float4(Normal, 1.0f);
	if(input.Position.z == 0.3f)
		return float4(Position, 1.0f);

	return float4(1,1,1,1);
}