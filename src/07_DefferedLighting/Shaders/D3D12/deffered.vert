struct VSOutput {
	float4 Position : SV_POSITION;
    float2 TexCoord : TEXCOORD;
};

VSOutput main(uint VertexID: SV_VertexID)
{
	VSOutput result;
	// Produce a fullscreen triangle
	float4 position;
	position.x = (VertexID == 2) ? 3.0 : -1.0;
	position.y = (VertexID == 0) ? -3.0 : 1.0;
	position.zw = 1.0;

	result.Position = position;
	result.TexCoord = position.xy * float2(0.5, -0.5) + 0.5;
	return result;
}