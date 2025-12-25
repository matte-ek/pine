#version 420 core

layout(location = 0) in vec3 vertex;

struct Instance
{
	mat4 transformationMatrix;
	ivec4 lightIndices;
};

layout(std140) uniform Instances 
{
	Instance instances[128];
};

#shader hooks

void main()
{
	vec4 vertexPosition = vec4(vertex, 1.0);
	mat4 transformationMatrix = instances[gl_InstanceID].transformationMatrix;

	#shader preVertex

	gl_Position = transformationMatrix * vertexPosition;

	#shader postVertex
}