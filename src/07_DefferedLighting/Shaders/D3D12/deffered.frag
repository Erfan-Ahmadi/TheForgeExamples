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

StructuredBuffer <DirectionalLight> DirectionalLights	: register(t5, UPDATE_FREQ_PER_FRAME);
StructuredBuffer <PointLight>		PointLights			: register(t6, UPDATE_FREQ_PER_FRAME);
StructuredBuffer <SpotLight>		SpotLights			: register(t7, UPDATE_FREQ_PER_FRAME);

cbuffer LightData : register(b1, UPDATE_FREQ_PER_FRAME)
{
	int numDirectionalLights;
	int numPointLights;
	int numSpotLights;
	float3 viewPos;
};

Texture2D			depthBuffer		: register(t1, UPDATE_FREQ_PER_FRAME);
Texture2D			albedoSpec		: register(t2, UPDATE_FREQ_PER_FRAME);
Texture2D			normal			: register(t3, UPDATE_FREQ_PER_FRAME);
Texture2D			position		: register(t4, UPDATE_FREQ_PER_FRAME);
SamplerState		uSampler0	: register(s0);

struct VSOutput 
{
	float4 Position		: SV_POSITION;
    float2 TexCoord		: TEXCOORD;
};

float linearaizeDepthValue(float2 uv)
{
    float depth = depthBuffer.Sample(uSampler0, uv).x;
	float f = 100;
	float n = 0.1f;
    return (2.0f * n) / (f + n - depth * (f - n));
}

float3 calculateDirectionalLight(DirectionalLight dirLight, float3 normal, float3 viewDir, float3 albedo, float specular);
float3 calculatePointLight(PointLight pointLight, float3 normal, float3 viewDir, float3 fragPosition, float3 albedo, float specular);
float3 calculateSpotLight(SpotLight spotLight, float3 normal, float3 viewDir, float3 fragPosition, float3 albedo, float specular);

float4 main(VSOutput input) : SV_TARGET
{
	// Uncomment next 2 lines if you want to visualize depth buffer
	/*float depth = linearaizeDepthValue(input.TexCoord);
	return float4(depth, depth, depth, 1);*/

    float3 Albedo = albedoSpec.Sample(uSampler0, input.TexCoord).rgb;
    float3 Normal = normal.Sample(uSampler0, input.TexCoord).rgb;
    float3 Position = position.Sample(uSampler0, input.TexCoord).rgb;
    float Specular = albedoSpec.Sample(uSampler0, input.TexCoord).a;
	
	float3 normal = normalize(Normal);
	float3 viewDir = normalize(viewPos - Position);

	float3 result;
	
	for(int i = 0; i < numDirectionalLights; ++i)
		result += calculateDirectionalLight(DirectionalLights[i], normal, viewDir, Albedo, Specular);
	for(int i = 0; i < numPointLights; ++i)
		result += calculatePointLight(PointLights[i], normal, viewDir, Position, Albedo, Specular);
	for(int i = 0; i < numSpotLights; ++i)
		result += calculateSpotLight(SpotLights[i], normal, viewDir, Position, Albedo, Specular);

    return float4(result, 1.0f);
}

float3 calculateDirectionalLight(DirectionalLight dirLight, float3 normal, float3 viewDir, float3 albedo, float specular)
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
	float3 specularLight = spec * dirLight.specular;  

	return (ambient + diffuse) * albedo + (specularLight) * float3(specular, specular, specular);
}

float3 calculatePointLight(PointLight pointLight, float3 normal, float3 viewDir, float3 fragPosition, float3 albedo, float specular)
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
	float spec = pow(max(dot(reflectDir, viewDir), 0.0), 32);
	float3 specularLight = spec * pointLight.specular;  
	
	return attenuation * ((ambient + diffuse) * albedo + (specularLight) * float3(specular, specular, specular));
}

float3 calculateSpotLight(SpotLight spotLight, float3 normal, float3 viewDir, float3 fragPosition, float3 albedo, float specular)
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
	float spec = pow(max(dot(halfwayDir, normal), 0.0), 32);
	float3 specularLight = spec * spotLight.specular * intensity;  
	
	return attenuation * ((ambient + diffuse) * albedo + (specularLight) * float3(specular, specular, specular));
}