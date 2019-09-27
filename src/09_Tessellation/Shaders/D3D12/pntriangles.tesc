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

// PN patch data
struct PnPatch
{
	float b210;
	float b120;
	float b021;
	float b012;
	float b102;
	float b201;
	float b111;
	float n110;
	float n011;
	float n101;
};

// Hull Shader Output control point
struct HullOut
{
	float4	Position	: POSITION;
	float3	Normal		: NORMAL;
	PnPatch	PatchData	: PATCH;
};

float wij(int i, int j, InputPatch<VS_CONTROL_POINT_OUTPUT, 3> ip)
{
	return dot(ip[j].Position.xyz - ip[i].Position.xyz, ip[i].Normal);
}

float vij(int i, int j, InputPatch<VS_CONTROL_POINT_OUTPUT, 3> ip)
{
	float3 Pj_minus_Pi = ip[j].Position.xyz - ip[i].Position.xyz;
	float3 Ni_plus_Nj  = ip[i].Normal + ip[j].Normal;

	return 2.0 * dot(Pj_minus_Pi, Ni_plus_Nj) / dot(Pj_minus_Pi, Pj_minus_Pi);
}

[domain("tri")]
[partitioning("fractional_odd")]
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

	// set base 
	float P0 = ip[0].Position[invocationID];
	float P1 = ip[1].Position[invocationID];
	float P2 = ip[2].Position[invocationID];
	float N0 = ip[0].Normal[invocationID];
	float N1 = ip[1].Normal[invocationID];
	float N2 = ip[2].Normal[invocationID];

	// compute control points
	Output.PatchData.b210 = (2.0*P0 + P1 - wij(0, 1, ip)*N0)/3.0;
	Output.PatchData.b120 = (2.0*P1 + P0 - wij(1, 0, ip)*N1)/3.0;
	Output.PatchData.b021 = (2.0*P1 + P2 - wij(1, 2, ip)*N1)/3.0;
	Output.PatchData.b012 = (2.0*P2 + P1 - wij(2, 1, ip)*N2)/3.0;
	Output.PatchData.b102 = (2.0*P2 + P0 - wij(2, 0, ip)*N2)/3.0;
	Output.PatchData.b201 = (2.0*P0 + P2 - wij(0, 2, ip)*N0)/3.0;

	float E = ( Output.PatchData.b210
			+ Output.PatchData.b120
			+ Output.PatchData.b021
			+ Output.PatchData.b012
			+ Output.PatchData.b102
			+ Output.PatchData.b201 ) / 6.0;

	float V = (P0 + P1 + P2)/3.0;
	Output.PatchData.b111 = E + (E - V)*0.5;
	Output.PatchData.n110 = N0+N1-vij(0, 1, ip)*(P1-P0);
	Output.PatchData.n011 = N1+N2-vij(1, 2, ip)*(P2-P1);
	Output.PatchData.n101 = N2+N0-vij(2, 0, ip)*(P0-P2);

	return Output;
}

