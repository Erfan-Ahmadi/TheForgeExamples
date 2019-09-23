Texture2D			Texture			: register(t1);
Texture2D			TextureSpecular	: register(t2);
SamplerState		uSampler0	: register(s0);

struct VSOutput 
{
	float4 Normal		: NORMAL;
	float4 FragPos		: POSITION;
    float2 TexCoord		: TEXCOORD;
	float4 Position		: SV_POSITION;
};

struct PSOut
{
    float4 albedoSpec	: SV_Target0;
    float4 normal		: SV_Target1;
    float4 position		: SV_Target2;
};


PSOut main(VSOutput input) : SV_TARGET
{
	PSOut result;
    result.albedoSpec.xyz = Texture.Sample(uSampler0, input.TexCoord).xyz;
    result.albedoSpec.w = TextureSpecular.Sample(uSampler0, input.TexCoord).x;
    result.normal = normalize(input.Normal);
    result.position = input.FragPos;
	return result;
}