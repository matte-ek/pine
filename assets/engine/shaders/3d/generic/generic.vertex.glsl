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
	
	// Extract the camera origin from the view matrix, and calculate the direction from the vertex.
	vec3 cameraDir = normalize(vOut.cameraPos - vOut.worldPosition.xyz);	

	// Pass everything directly in world space
	vOut.lightDir[0] = normalize(lights[0].rotation);

    for (int i = 1; i < 6;i++)
    {
    	vOut.lightDir[i] = normalize(lights[vOut.lightIndices[i - 1]].position - vOut.worldPosition.xyz);
    }

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

		for (int i = 1; i < 6;i++)
		{
			vOut.lightDir[i] = tangentMatrix * vOut.lightDir[i];
		}	

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