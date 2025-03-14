#version 420 core

layout(location = 0) in vec3 vertex;
layout(location = 1) in vec3 normal;
layout(location = 2) in vec2 uv;
layout(location = 3) in vec3 tangent;

struct Light 
{
	vec3 position;
	vec3 rotation;
	vec3 color;
	vec3 attenuation;
	float cutOffAngle;
	float cutOffSmoothness;
};

layout(std140) uniform Matrices
{
	mat4 projectionMatrix;
	mat4 viewMatrix;
};

layout(std140) uniform Transform 
{
	mat4 transformationMatrices[128];
};

layout(std140) uniform Lights 
{
	Light lights[32];
};

out VertexData
{
	vec2 uv;
	vec3 worldPosition;
	vec3 cameraPos;
	vec3 cameraDir;
	vec3 normalDir;
	vec3 lightDir[4];
}vOut;

uniform bool hasTangentData;

// Light indices from Lights that should affect this object
uniform ivec4 lightsIndices;

#shader hooks

void main()
{
	vec4 vertexPosition = vec4(vertex, 1.0);
	mat4 transformationMatrix = transformationMatrices[gl_InstanceID];

	#shader preVertex

	vOut.worldPosition = (transformationMatrix * vertexPosition).xyz;
	vOut.uv = uv;
	vOut.cameraPos = (inverse(viewMatrix) * vec4(0.0, 0.0, 0.0, 1.0)).xyz;

	// Apply object transformation to our normal vector
	vec3 worldNormalDir = normalize((transformationMatrix * vec4(normal, 0.0)).xyz);
	
	// Extract the camera origin from the view matrix, and calculate the direction from the vertex.
	vec3 cameraDir = normalize(vOut.cameraPos - vOut.worldPosition.xyz);	

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

		vOut.lightDir[0] = tangentMatrix * normalize(lights[0].rotation);

		for (int i = 1; i < 4;i++)
		{
			vOut.lightDir[i] = tangentMatrix * normalize(lights[i].position - vOut.worldPosition.xyz);
		}	

		vOut.cameraDir = tangentMatrix * cameraDir;
		vOut.normalDir = tangentMatrix * worldNormalDir;
	}
	else
	{
		// Pass everything directly in world space
		vOut.lightDir[0] = normalize(lights[0].rotation);

		for (int i = 1; i < 4;i++)
		{
			vOut.lightDir[i] = normalize(lights[i].position - vOut.worldPosition.xyz);
		}	

		vOut.cameraDir = cameraDir;
		vOut.normalDir = worldNormalDir;
	}

	gl_Position = projectionMatrix * viewMatrix * transformationMatrix * vertexPosition;

	#shader postVertex
}