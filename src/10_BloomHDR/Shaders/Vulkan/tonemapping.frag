#version 450 core

#define TEX_DIM 256

layout(location = 0) in vec2 UV;

layout (binding = 1) uniform sampler								uSampler0;
layout (UPDATE_FREQ_PER_FRAME,  binding = 2) uniform texture2D		HdrTexture;
layout (UPDATE_FREQ_PER_FRAME,  binding = 3) uniform texture2D		BloomTexture;

layout(UPDATE_FREQ_PER_FRAME, binding = 0) uniform ToneMappingData
{
	float inExposure;
	float inGamma;
	float bloomLevel;
	bool tonemap;
};

layout(location = 0) out vec4 outColor;

void main()
{
	vec3 bloom = texture(sampler2D(BloomTexture, uSampler0), UV).rgb;
	outColor = vec4(bloom, 1.0f);
	return;

	const float gamma = inGamma;
  
	vec4 hdrColor = texture(sampler2D(HdrTexture, uSampler0), UV) + bloomLevel * vec4(bloom, 1.0f);
	vec3 mapped;

	if(tonemap)
	{
		// reinhard tone mapping
		mapped = hdrColor.xyz / (hdrColor.xyz + 1.0);
	}
	else
	{
		// Exposure tone mapping
		mapped = 1.0f - exp2(-hdrColor.xyz * inExposure);
	}
	
    // Gamma correction 
    mapped = pow(mapped, vec3(1.0 / gamma));

    outColor = vec4(mapped, hdrColor.a);
}