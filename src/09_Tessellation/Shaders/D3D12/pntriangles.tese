cbuffer UniformData : register(b0, UPDATE_FREQ_PER_FRAME)
{
	float4x4	world;
	float4x4	view;
	float4x4	proj;
	float		tessellationLevel;
    float		tessAlpha;
};

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

float4x4 inverse(float4x4 input);

[domain("tri")]
[partitioning("fractional_odd")]
[outputtopology("triangle_ccw")]
DS_OUTPUT main(HS_CONSTANT_DATA_OUTPUT input, float3 location : SV_DomainLocation, OutputPatch<HullOut, 3> patch)
{
    DS_OUTPUT Output;

	float3 uvw			= location;
    float3 uvwSquared	= uvw * uvw;
    float3 uvwCubed		= uvwSquared * uvw;

    // extract control points
    float3 b210 = float3(patch[0].PatchData.b210, patch[1].PatchData.b210, patch[2].PatchData.b210);
    float3 b120 = float3(patch[0].PatchData.b120, patch[1].PatchData.b120, patch[2].PatchData.b120);
    float3 b021 = float3(patch[0].PatchData.b021, patch[1].PatchData.b021, patch[2].PatchData.b021);
    float3 b012 = float3(patch[0].PatchData.b012, patch[1].PatchData.b012, patch[2].PatchData.b012);
    float3 b102 = float3(patch[0].PatchData.b102, patch[1].PatchData.b102, patch[2].PatchData.b102);
    float3 b201 = float3(patch[0].PatchData.b201, patch[1].PatchData.b201, patch[2].PatchData.b201);
    float3 b111 = float3(patch[0].PatchData.b111, patch[1].PatchData.b111, patch[2].PatchData.b111);

    // extract control normals
    float3 n110 = normalize(float3(patch[0].PatchData.n110, patch[1].PatchData.n110, patch[2].PatchData.n110));
    float3 n011 = normalize(float3(patch[0].PatchData.n011, patch[1].PatchData.n011, patch[2].PatchData.n011));
    float3 n101 = normalize(float3(patch[0].PatchData.n101, patch[1].PatchData.n101, patch[2].PatchData.n101));

    // normal
    // Barycentric normal
    float3 barNormal = location[2]*patch[0].Normal + location[0]*patch[1].Normal + location[1]*patch[2].Normal;
    float3 pnNormal  = patch[0].Normal*uvwSquared[2] + patch[1].Normal*uvwSquared[0] + patch[2].Normal*uvwSquared[1]
                   + n110*uvw[2]*uvw[0] + n011*uvw[0]*uvw[1]+ n101*uvw[2]*uvw[1];
    float3 normal = tessAlpha*pnNormal + (1.0-tessAlpha) * barNormal;
	Output.Normal = mul(transpose(inverse(world)), normal);

    // compute interpolated pos
    float3 barPos = location[2]*patch[0].Position.xyz
                + location[0]*patch[1].Position.xyz
                + location[1]*patch[2].Position.xyz;

    // save some computations
    uvwSquared *= 3.0;

    // compute PN position
    float3 pnPos  = patch[0].Position.xyz*uvwCubed[2]
                + patch[1].Position.xyz*uvwCubed[0]
                + patch[2].Position.xyz*uvwCubed[1]
                + b210*uvwSquared[2]*uvw[0]
                + b120*uvwSquared[0]*uvw[2]
                + b201*uvwSquared[2]*uvw[1]
                + b021*uvwSquared[0]*uvw[1]
                + b102*uvwSquared[1]*uvw[2]
                + b012*uvwSquared[1]*uvw[0]
                + b111*6.0*uvw[0]*uvw[1]*uvw[2];

    // final position and normal
    float3 finalPos = (1.0 - tessAlpha) * barPos + tessAlpha * pnPos;
	Output.Position = mul(proj, mul(view, mul(world, float4(finalPos, 1.0))));

    return Output;
}

float4x4 inverse(float4x4 input)
{
    #define minor(a,b,c) determinant(float3x3(input.a, input.b, input.c))
    //determinant(float3x3(input._22_23_23, input._32_33_34, input._42_43_44))
    
    float4x4 cofactors = float4x4(
         minor(_22_23_24, _32_33_34, _42_43_44), 
        -minor(_21_23_24, _31_33_34, _41_43_44),
         minor(_21_22_24, _31_32_34, _41_42_44),
        -minor(_21_22_23, _31_32_33, _41_42_43),
        
        -minor(_12_13_14, _32_33_34, _42_43_44),
         minor(_11_13_14, _31_33_34, _41_43_44),
        -minor(_11_12_14, _31_32_34, _41_42_44),
         minor(_11_12_13, _31_32_33, _41_42_43),
        
         minor(_12_13_14, _22_23_24, _42_43_44),
        -minor(_11_13_14, _21_23_24, _41_43_44),
         minor(_11_12_14, _21_22_24, _41_42_44),
        -minor(_11_12_13, _21_22_23, _41_42_43),
        
        -minor(_12_13_14, _22_23_24, _32_33_34),
         minor(_11_13_14, _21_23_24, _31_33_34),
        -minor(_11_12_14, _21_22_24, _31_32_34),
         minor(_11_12_13, _21_22_23, _31_32_33)
    );
    #undef minor
    return transpose(cofactors) / determinant(input);
}