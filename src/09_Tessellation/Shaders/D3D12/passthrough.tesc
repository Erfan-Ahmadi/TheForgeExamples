cbuffer UniformData : register(b0, UPDATE_FREQ_PER_FRAME)
{
	float4x4	world;
	float4x4	view;
	float4x4	proj;
	float		tessellationLevel;
};

struct VS_CONTROL_POINT_OUTPUT 
{
	float4 Position : POSITION;
	float3 Normal	: NORMAL;
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

// Patch Constant Function
HS_CONSTANT_DATA_OUTPUT PatchConstantFunction(InputPatch<VS_CONTROL_POINT_OUTPUT, 3> ip, uint PatchID : SV_PrimitiveID)
{
	HS_CONSTANT_DATA_OUTPUT Output;

	Output.Inside[0] = tessellationLevel;

	Output.Edges[0] = tessellationLevel;
	Output.Edges[1] = tessellationLevel;
	Output.Edges[2] = tessellationLevel;
	
	return Output;
}

[domain("tri")]
[partitioning("integer")]
[outputtopology("triangle_ccw")]
[outputcontrolpoints(3)]
[patchconstantfunc("PatchConstantFunction")]
[maxtessfactor(10.0f)]
HullOut main(
	InputPatch<VS_CONTROL_POINT_OUTPUT, 3> ip,
	uint invocationID : SV_OutputControlPointID)
{
	HullOut Output;

	Output.Position = ip[invocationID].Position;
	Output.Normal = ip[invocationID].Normal;

	return Output;
}

