#version 450 core

layout(location = 0) out vec2 outTexCoords;

void main(void)
{	
	vec4 p;
	uint id = gl_VertexIndex;
	p.x = (id == 2) ? 3.0 : -1.0;
	p.y = (id == 0) ? 3.0 : -1.0;
	p.z = 1.0;
	p.w = 1.0;

	gl_Position = p.xyzw;
	outTexCoords = p.xy * vec2(0.5, -0.5) + 0.5;
}