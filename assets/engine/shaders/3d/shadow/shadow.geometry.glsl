#version 420 core

layout(triangles, invocations = 4) in;
layout(triangle_strip, max_vertices = 3) out;

layout(std140) uniform Shadows
{
	mat4 lightSpaceMatrix[8];
};

#shader hooks

void main()
{
	for (int i = 0; i < 3;i++)
	{
		gl_Position = lightSpaceMatrix[gl_InvocationID] * gl_in[i].gl_Position;
		gl_Layer = gl_InvocationID;

		EmitVertex();
	}

	EndPrimitive();
}