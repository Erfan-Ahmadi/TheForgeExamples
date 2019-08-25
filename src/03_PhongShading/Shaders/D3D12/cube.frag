
cbuffer LightData : register(b0)
{
	float3 lightPos;
	float3 lightColor;
	float3 viewPos;
};

struct VSOutput {
	float4 Normal		: NORMAL;
	float4 FragPos		: POSITION;
	float4 Position		: SV_POSITION;
    float2 TexCoord		: TEXCOORD;
};

SamplerState	uSampler0	: register(s2);
Texture2D		Texture		: register(t1);

float4 main(VSOutput input) : SV_TARGET
{
	float3 normal = normalize(input.Normal.xyz);
	float3 lightDir = normalize(lightPos - input.FragPos.xyz);
	
	float3 viewDir = normalize(viewPos - input.FragPos.xyz);
	float3 reflectDir = reflect(-lightDir, normal);

	// Ambient
	float ambientStrength = 0.2f;
    float3 ambient = ambientStrength * lightColor;

	// Diffuse
	float diff = max(dot(lightDir, normal), 0.0f);
	float3 diffuse = diff * lightColor;

	// Specular
	float specularStrenght = 1.5f;
	float spec = pow(max(dot(reflectDir, viewDir), 0.0), 64);
	float3 specular = specularStrenght * spec * lightColor;  

	float3 color = (ambient + diffuse + specular) * float3(0.3f, 0.3, 0.5f);//(Texture.Sample(uSampler0, input.TexCoord).xyz);

    return float4(color, 1.0f);
}