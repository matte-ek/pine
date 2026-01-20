#version 420 core

layout(location = 0) in vec3 vertex;

#include "shared/common.glsl"

#shader hooks

void main()
{
	vec4 vertexPosition = vec4(vertex, 1.0);
	mat4 transformationMatrix = instances[gl_InstanceID].transformationMatrix;

	#shader preVertex

	gl_Position = transformationMatrix * vertexPosition;

	#shader postVertex
}