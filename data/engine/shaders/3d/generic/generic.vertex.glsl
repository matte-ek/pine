#version 420 core

#define PINE_VERTEX_STAGE

layout(location = 0) in vec3 vertex;
layout(location = 1) in vec3 normal;
layout(location = 2) in vec2 uv;
layout(location = 3) in vec3 tangent;

#include "shared/common.glsl"
#include "shared/vertex-data.glsl"

uniform bool hasTangentData;

#shader hooks

void main()
{
	vec4 vertexPosition = vec4(vertex, 1.0);

	mat4 transformationMatrix = instances[gl_InstanceID].transformationMatrix;

	#shader preVertex

	vOut.worldPosition = (transformationMatrix * vertexPosition).xyz;
	vOut.uv = uv;
	vOut.cameraPos = (inverse(viewMatrix) * vec4(0.0, 0.0, 0.0, 1.0)).xyz;

	writeLightIndices();

	// Apply object transformation to our normal vector
	vec3 worldNormalDir = normalize((transformationMatrix * vec4(normal, 0.0)).xyz);

	vOut.worldNormal = worldNormalDir;

	// Extract the camera origin from the view matrix, and calculate the direction from the vertex.
	vec3 cameraDir = normalize(vOut.cameraPos - vOut.worldPosition.xyz);

	writeLightDirections();

	if (hasTangentData)
	{
		vec3 worldTangent = normalize((transformationMatrix * vec4(tangent, 0.0)).xyz);

		// Magic matrix we can multiply vectors with to convert them into
		// tangent space.
		mat3 tangentMatrix = CreateTangentMatrix(worldNormalDir, worldTangent);

		transformLightDirections(tangentMatrix);

		vOut.cameraDir = tangentMatrix * cameraDir;
		vOut.normalDir = tangentMatrix * worldNormalDir;
	}
	else
	{
		vOut.cameraDir = cameraDir;
		vOut.normalDir = worldNormalDir;
	}

	gl_Position = projectionMatrix * viewMatrix * transformationMatrix * vertexPosition;

	#shader postVertex
}
