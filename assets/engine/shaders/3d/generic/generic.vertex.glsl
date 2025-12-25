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

struct Instance
{
	mat4 transformationMatrix;
	ivec4 lightIndices;
};

layout(std140) uniform Matrices
{
	mat4 projectionMatrix;
	mat4 viewMatrix;
};

layout(std140) uniform Instances 
{
	Instance instances[128];
};

layout(std140) uniform Lights 
{
	Light lights[32];
    vec3 worldAmbientColor;
};

out VertexData
{
	vec2 uv;
	vec3 worldPosition;
	vec3 cameraPos;
	vec3 cameraDir;
	vec3 normalDir;
	vec3 lightDir[5];
    flat ivec4 lightIndices;
}vOut;

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
    vOut.lightIndices = instances[gl_InstanceID].lightIndices;

	// Apply object transformation to our normal vector
	vec3 worldNormalDir = normalize((transformationMatrix * vec4(normal, 0.0)).xyz);
	
	// Extract the camera origin from the view matrix, and calculate the direction from the vertex.
	vec3 cameraDir = normalize(vOut.cameraPos - vOut.worldPosition.xyz);	

	// Pass everything directly in world space
	vOut.lightDir[0] = normalize(lights[0].rotation);

	vOut.lightDir[1] = normalize(lights[vOut.lightIndices.x].position - vOut.worldPosition.xyz);
	vOut.lightDir[2] = normalize(lights[vOut.lightIndices.y].position - vOut.worldPosition.xyz);
	vOut.lightDir[3] = normalize(lights[vOut.lightIndices.z].position - vOut.worldPosition.xyz);
	vOut.lightDir[4] = normalize(lights[vOut.lightIndices.w].position - vOut.worldPosition.xyz);

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

		for (int i = 1; i < 4;i++)
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