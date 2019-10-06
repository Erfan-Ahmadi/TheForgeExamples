#version 450 core

#define TEX_DIM 256

layout(location = 0) in vec2 UV;

layout (binding = 1) uniform sampler								uSampler0;
layout (UPDATE_FREQ_PER_FRAME, binding = 2) uniform texture2D		Texture;

layout(location = 0) out vec4 outColor;

void main()
{
	// Horizontal Blur
	float weight[5];
	weight[0] = 0.227027;
	weight[1] = 0.1945946;
	weight[2] = 0.1216216;
	weight[3] = 0.054054;
	weight[4] = 0.016216;	
	
	vec2 tex_offset = vec2(1.0f / TEX_DIM);
	vec3 result = texture(sampler2D(Texture, uSampler0), UV).rgb * weight[0];

	for(int i = 0; i < 5; ++i)
	{
		result += texture(sampler2D(Texture, uSampler0), UV + vec2(tex_offset.x * i, 0.0)).rgb * weight[i] * 1.5f;
		result += texture(sampler2D(Texture, uSampler0), UV - vec2(tex_offset.x * i, 0.0)).rgb * weight[i] * 1.5f;
	}
	
	outColor = vec4(result, 1.0);
}