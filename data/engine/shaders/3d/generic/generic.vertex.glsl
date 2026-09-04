#version 420 core

layout(location = 0) in vec3 vertex;
layout(location = 1) in vec3 normal;
layout(location = 2) in vec2 uv;
layout(location = 3) in vec3 tangent;

#include "shared/common.glsl"

out VertexData
{
	vec2 uv;
	vec3 worldPosition;
	vec3 cameraPos;
	vec3 cameraDir;
	float cameraDistance;
	vec3 normalDir;

	// Always world space, unlike normalDir which is rotated into tangent space for normal-mapped
	// materials. Shadow lookups need a normal they can offset a world position along, so they
	// cannot use normalDir.
	vec3 worldNormal;

	vec3 lightDir[8];
    flat int lightIndices[8];
}vOut;

uniform bool hasTangentData;

#shader hooks

void writeLightIndices()
{
    vOut.lightIndices[0] = instances[gl_InstanceID].lightIndices[0].x;
    vOut.lightIndices[1] = instances[gl_InstanceID].lightIndices[0].y;
    vOut.lightIndices[2] = instances[gl_InstanceID].lightIndices[0].z;
    vOut.lightIndices[3] = instances[gl_InstanceID].lightIndices[0].w;

    vOut.lightIndices[4] = instances[gl_InstanceID].lightIndices[1].x;
    vOut.lightIndices[5] = instances[gl_InstanceID].lightIndices[1].y;
    vOut.lightIndices[6] = instances[gl_InstanceID].lightIndices[1].z;
    vOut.lightIndices[7] = instances[gl_InstanceID].lightIndices[1].w;
}

void main()
{
	vec4 vertexPosition = vec4(vertex, 1.0);
	mat4 transformationMatrix = instances[gl_InstanceID].transformationMatrix;

	#shader preVertex

	vOut.worldPosition = (transformationMatrix * vertexPosition).xyz;
	vOut.uv = uv;
	vOut.cameraPos = (inverse(viewMatrix) * vec4(0.0, 0.0, 0.0, 1.0)).xyz;
	vOut.cameraDistance = length(vOut.worldPosition - vOut.cameraPos);

	writeLightIndices();

	// Apply object transformation to our normal vector
	vec3 worldNormalDir = normalize((transformationMatrix * vec4(normal, 0.0)).xyz);

	vOut.worldNormal = worldNormalDir;
	
	// Extract the camera origin from the view matrix, and calculate the direction from the vertex.
	vec3 cameraDir = normalize(vOut.cameraPos - vOut.worldPosition.xyz);	

	// Pass everything directly in world space.
	// lightDir[0] is the directional light; lightDir[n] (n = 1..7) is the direction towards the light in
	// instance light slot n - 1 (slots 0-4 point lights, slots 5-6 spot lights). Written with literal
	// subscripts: dynamically indexing this varying array misbehaves on some drivers (NVIDIA).
	// lightDir[] is sized 8, so slot 6 is the last one that fits without growing it.
	vOut.lightDir[0] = normalize(lights[0].directionToLight);
	vOut.lightDir[1] = normalize(lights[vOut.lightIndices[0]].position - vOut.worldPosition.xyz);
	vOut.lightDir[2] = normalize(lights[vOut.lightIndices[1]].position - vOut.worldPosition.xyz);
	vOut.lightDir[3] = normalize(lights[vOut.lightIndices[2]].position - vOut.worldPosition.xyz);
	vOut.lightDir[4] = normalize(lights[vOut.lightIndices[3]].position - vOut.worldPosition.xyz);
	vOut.lightDir[5] = normalize(lights[vOut.lightIndices[4]].position - vOut.worldPosition.xyz);
	vOut.lightDir[6] = normalize(lights[vOut.lightIndices[5]].position - vOut.worldPosition.xyz);
	vOut.lightDir[7] = normalize(lights[vOut.lightIndices[6]].position - vOut.worldPosition.xyz);

	if (hasTangentData)
	{
		vec3 worldTangent = normalize((transformationMatrix * vec4(tangent, 0.0)).xyz);
		vec3 worldBiTangent = normalize(cross(worldNormalDir, worldTangent));

		// Magic matrix we can multiply vectors with to convert them into
		// tangent space.
		mat3 tangentMatrix = mat3(
			worldTangent.x, worldBiTangent.x, worldNormalDir.x,
			worldTangent.y, worldBiTangent.y, worldNormalDir.y,
			worldTangent.z, worldBiTangent.z, worldNormalDir.z
		);

		vOut.lightDir[0] = tangentMatrix * vOut.lightDir[0];
		vOut.lightDir[1] = tangentMatrix * vOut.lightDir[1];
		vOut.lightDir[2] = tangentMatrix * vOut.lightDir[2];
		vOut.lightDir[3] = tangentMatrix * vOut.lightDir[3];
		vOut.lightDir[4] = tangentMatrix * vOut.lightDir[4];
		vOut.lightDir[5] = tangentMatrix * vOut.lightDir[5];
		vOut.lightDir[6] = tangentMatrix * vOut.lightDir[6];
		vOut.lightDir[7] = tangentMatrix * vOut.lightDir[7];

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