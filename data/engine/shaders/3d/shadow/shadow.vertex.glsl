#version 420 core

layout(location = 0) in vec3 vertex;
layout(location = 2) in vec2 uv;

#include "shared/common.glsl"

#ifdef VERSION_DISCARD
out VertexData
{
    vec2 uv;
}vOut;
#endif

#shader hooks

void main()
{
	vec4 vertexPosition = vec4(vertex, 1.0);
	mat4 transformationMatrix = instances[gl_InstanceID].transformationMatrix;

	#shader preVertex

	gl_Position = projectionMatrix * viewMatrix * transformationMatrix * vertexPosition;

#ifdef VERSION_DISCARD
	vOut.uv = uv;
#endif

	#shader postVertex
}
