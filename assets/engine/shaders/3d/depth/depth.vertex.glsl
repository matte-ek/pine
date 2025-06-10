#version 420 core

layout(location = 0) in vec3 vertex;
layout(location = 1) in vec3 normal;

layout(std140) uniform Matrices
{
	mat4 projectionMatrix;
	mat4 viewMatrix;
};

layout(std140) uniform Transform 
{
	mat4 transformationMatrices[128];
};

out VertexData
{
    vec3 normal;
}vOut;

#shader hooks

void main()
{
	vec4 vertexPosition = vec4(vertex, 1.0);
	mat4 transformationMatrix = transformationMatrices[gl_InstanceID];

	mat3 normalMatrix = transpose(inverse(mat3(viewMatrix * transformationMatrix)));

	vOut.normal = normalMatrix * normal;

	#shader preVertex

	gl_Position = projectionMatrix * viewMatrix * transformationMatrix * vertexPosition;

	#shader postVertex
}