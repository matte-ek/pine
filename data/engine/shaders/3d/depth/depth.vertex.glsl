#version 420 core

layout(location = 0) in vec3 vertex;
layout(location = 1) in vec3 normal;

#include "shared/common.glsl"

out VertexData
{
    vec3 normal;
}vOut;

#shader hooks

void main()
{
	vec4 vertexPosition = vec4(vertex, 1.0);
	mat4 transformationMatrix = instances[gl_InstanceID].transformationMatrix;

	mat3 normalMatrix = transpose(inverse(mat3(viewMatrix * transformationMatrix)));

	vOut.normal = normalMatrix * normal;

	#shader preVertex

	gl_Position = projectionMatrix * viewMatrix * transformationMatrix * vertexPosition;

	#shader postVertex
}