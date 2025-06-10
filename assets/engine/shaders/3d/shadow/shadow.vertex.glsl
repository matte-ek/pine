#version 420 core

layout(location = 0) in vec3 vertex;

layout(std140) uniform Transform 
{
	mat4 transformationMatrices[128];
};

#shader hooks

void main()
{
	vec4 vertexPosition = vec4(vertex, 1.0);
	mat4 transformationMatrix = transformationMatrices[gl_InstanceID];

	#shader preVertex

	gl_Position = transformationMatrix * vertexPosition;

	#shader postVertex
}