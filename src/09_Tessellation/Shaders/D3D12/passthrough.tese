cbuffer UniformData : register(b0, UPDATE_FREQ_PER_FRAME)
{
	float4x4	world;
	float4x4	view;
	float4x4	proj;
	float		tessellationLevel;
};

// Hull Shader Output control point
struct HullOut
{
	float4 Position : POSITION;
	float3 Normal	: NORMAL;
};

// Output patch constant data.
struct HS_CONSTANT_DATA_OUTPUT
{
	float Edges[3]        : SV_TessFactor;
	float Inside[1]       : SV_InsideTessFactor;	
};

struct DS_OUTPUT
{
    float4 Position : SV_Position;
    float3 Normal : NORMAL;
};

[domain("tri")]
[partitioning("integer")]
[outputtopology("triangle_ccw")]
DS_OUTPUT main(HS_CONSTANT_DATA_OUTPUT input, float3 location : SV_DomainLocation, OutputPatch<HullOut, 3> patch)
{
    DS_OUTPUT Output;

	float3 finalPos =	(	location.x * patch[0].Position +
							location.y * patch[1].Position +
							location.z * patch[2].Position);

	Output.Position = mul(proj, mul(view, mul(world, float4(finalPos, 1.0f))));
	
	Output.Normal =		(	location.x * patch[0].Normal +
							location.y * patch[1].Normal +
							location.z * patch[2].Normal);

    return Output;
}
