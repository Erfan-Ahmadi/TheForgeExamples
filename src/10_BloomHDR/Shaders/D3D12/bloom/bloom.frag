struct DirectionalLight 
{
    float3 direction;
    float3 ambient;
    float3 diffuse;
    float3 specular;
};

struct PointLight 
{
	float3 position;
	float3 ambient;
	float3 diffuse;
	float3 specular;
	
    float att_constant;
    float att_linear;
    float att_quadratic;
	float _pad0;
};

struct SpotLight 
{
	float3 position;
	float3 direction;
	float2 cutOff;
	float3 ambient;
	float3 diffuse;
	float3 specular;
    float3 att_params;
};

StructuredBuffer <DirectionalLight> DirectionalLights	: register(t0, UPDATE_FREQ_PER_FRAME);
StructuredBuffer <PointLight>		PointLights			: register(t1, UPDATE_FREQ_PER_FRAME);
StructuredBuffer <SpotLight>		SpotLights			: register(t2, UPDATE_FREQ_PER_FRAME);

cbuffer LightData : register(b1, UPDATE_FREQ_PER_FRAME)
{
	int numDirectionalLights;
	int numPointLights;
	int numSpotLights;
	float3 viewPos;
};

struct VSOutput 
{
	float4 Normal		: NORMAL;
	float4 FragPos		: POSITION;
	float4 Position		: SV_POSITION;
    float2 TexCoord		: TEXCOORD;
};

SamplerState	uSampler0		: register(s0);

float3 calculateDirectionalLight(DirectionalLight dirLight, float3 normal, float3 viewDir, float2 TexCoord);
float3 calculatePointLight(PointLight pointLight, float3 normal, float3 viewDir, float3 fragPosition, float2 TexCoord);
float3 calculateSpotLight(SpotLight spotLight, float3 normal, float3 viewDir, float3 fragPosition, float2 TexCoord);

struct PSOut
{
    float4 bright	: SV_Target0;
};

PSOut main(VSOutput input) : SV_TARGET
{
	PSOut output;

	float3 normal = normalize(input.Normal.xyz);
	float3 viewDir = normalize(viewPos - input.FragPos.xyz);
	
	float3 result;
	
	for(int i = 0; i < numDirectionalLights; ++i)
		result += calculateDirectionalLight(DirectionalLights[i], normal, viewDir, input.TexCoord);
	for(int i = 0; i < numPointLights; ++i)
		result += calculatePointLight(PointLights[i], normal, viewDir, input.FragPos.xyz, input.TexCoord);
	for(int i = 0; i < numSpotLights; ++i)
		result += calculateSpotLight(SpotLights[i], normal, viewDir, input.FragPos.xyz, input.TexCoord);
		
	// TODO:
	// 1. Texture 1D Lookup
	// 2. Set bloom limit in a uniform
	float luminance = dot(result, float3(0.2126, 0.7152, 0.0722));

	if(luminance > 1.0f)
		output.bright = float4(result, 1.0f);
    else
        output.bright = float4(0.0, 0.0, 0.0, 1.0);

	return output;
}

float3 calculateDirectionalLight(DirectionalLight dirLight, float3 normal, float3 viewDir, float2 TexCoord)
{
	float3 lightDir = normalize(-dirLight.direction);
	float3 reflectDir = reflect(-lightDir, normal);

	// Ambient
    float3 ambient = dirLight.ambient;

	// Diffuse
	float diff = max(dot(lightDir, normal), 0.0f);
	float3 diffuse = diff * dirLight.diffuse;

	// Specular
	float spec = pow(max(dot(reflectDir, viewDir), 0.0), 32);
	float3 specular = spec * dirLight.specular;  

	return (ambient + diffuse) + (specular);
}

float3 calculatePointLight(PointLight pointLight, float3 normal, float3 viewDir, float3 fragPosition, float2 TexCoord)
{
	float3 difference = pointLight.position - fragPosition;

	float3 lightDir = normalize(difference);
	float3 reflectDir = reflect(-lightDir, normal);

	// Calc Attenuation
	float distance = length(difference);
	float3 dis = float3(1, distance, pow(distance, 2));
	float3 att = float3(pointLight.att_constant, pointLight.att_linear, pointLight.att_quadratic);
	float attenuation = 1.0f / dot(dis, att);

	// Ambient
    float3 ambient = pointLight.ambient;

	// Diffuse
	float diff = max(dot(lightDir, normal), 0.0f);
	float3 diffuse = diff * pointLight.diffuse;

	// Specular
	float spec = pow(max(dot(reflectDir, viewDir), 0.0), 64);
	float3 specular = spec * pointLight.specular;  

	return attenuation * ((ambient + diffuse) + (specular));
}

float3 calculateSpotLight(SpotLight spotLight, float3 normal, float3 viewDir, float3 fragPosition, float2 TexCoord)
{
	float3 difference = spotLight.position - fragPosition;
	float3 lightDir	= normalize(difference);
	float3 reflectDir = reflect(-lightDir, normal);
	float3 halfwayDir = normalize(lightDir + viewDir);
	
	float theta = dot(normalize(-spotLight.direction), lightDir);
	float epsilon = spotLight.cutOff.x - spotLight.cutOff.y;
	float intensity = clamp((theta - spotLight.cutOff.y) / epsilon , 0.0f, 1.0f);
	
	// Calc Attenuation
	float distance = length(difference);
	float3 dis = float3(1, distance, pow(distance, 2));
	float attenuation = 1.0f / dot(dis, spotLight.att_params);

	// Ambient
    float3 ambient = spotLight.ambient;

	// Diffuse
	float diff = max(dot(lightDir, normal), 0.0f);
	float3 diffuse = diff * spotLight.diffuse * intensity;

	// Specular
	float spec = pow(max(dot(halfwayDir, normal), 0.0), 64);
	float3 specular = spec * spotLight.specular * intensity;  

	return attenuation * ((ambient + diffuse) + (specular));
}