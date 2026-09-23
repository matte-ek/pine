#version 430 core

#define PINE_VERTEX_STAGE

layout(location = 0) in vec3 vertex;
layout(location = 1) in vec3 normal;
layout(location = 2) in vec2 uv;
layout(location = 3) in vec3 tangent;

#include "shared/common.glsl"
#include "shared/vertex-data.glsl"

uniform bool hasTangentData;

// Terrain detail: thousands of copies of one mesh in a single draw, each placed by an entry in a
// storage buffer that is written once and kept, rather than by the Instances block, which is
// rewritten for every draw and holds MAX_INSTANCE_COUNT at most. instances[0] still carries the
// terrain's transform and the light slots the copies share. Needs GLSL 4.30 for the storage block.
#shader version VERSION_TERRAIN_DETAIL 8

#ifdef VERSION_TERRAIN_DETAIL

// Mirrors ShaderStorages::TerrainDetailInstanceData.
struct TerrainDetailInstance
{
	// xyz the terrain-local position, w the uniform scale.
	vec4 positionScale;

	// x = cos(yaw), y = sin(yaw).
	vec4 rotation;
};

layout(std430, binding = TERRAIN_DETAIL_INSTANCE_BINDING) readonly buffer TerrainDetailInstances
{
	TerrainDetailInstance detailInstances[];
};

// x = the camera distance at which a copy starts shrinking, y = the one at which it is gone.
uniform vec2 detailFade;

// This copy's placement within the terrain, shrunk towards nothing as it nears the draw distance
// so the edge of the detail sinks into the ground instead of popping.
mat4 GetDetailPlacement(mat4 terrainTransform, vec3 cameraPosition)
{
	TerrainDetailInstance detail = detailInstances[gl_InstanceID];

	vec3 worldOrigin = (terrainTransform * vec4(detail.positionScale.xyz, 1.0)).xyz;
	float fade = 1.0 - smoothstep(detailFade.x, detailFade.y, length(worldOrigin - cameraPosition));

	float scale = detail.positionScale.w * fade;
	float cosYaw = detail.rotation.x;
	float sinYaw = detail.rotation.y;

	// Columns: a rotation about +y, scaled, then the translation.
	return mat4(
		vec4(cosYaw * scale, 0.0, -sinYaw * scale, 0.0),
		vec4(0.0, scale, 0.0, 0.0),
		vec4(sinYaw * scale, 0.0, cosYaw * scale, 0.0),
		vec4(detail.positionScale.xyz, 1.0));
}

#endif

#shader hooks

void main()
{
	vec4 vertexPosition = vec4(vertex, 1.0);

#ifdef VERSION_TERRAIN_DETAIL
	mat4 terrainTransform = instances[0].transformationMatrix;
	vec3 cameraPosition = (inverse(viewMatrix) * vec4(0.0, 0.0, 0.0, 1.0)).xyz;

	mat4 transformationMatrix = terrainTransform * GetDetailPlacement(terrainTransform, cameraPosition);
#else
	mat4 transformationMatrix = instances[gl_InstanceID].transformationMatrix;
#endif

	#shader preVertex

	vOut.worldPosition = (transformationMatrix * vertexPosition).xyz;
	vOut.uv = uv;
	vOut.cameraPos = (inverse(viewMatrix) * vec4(0.0, 0.0, 0.0, 1.0)).xyz;
	vOut.cameraDistance = length(vOut.worldPosition - vOut.cameraPos);

#ifdef VERSION_TERRAIN_DETAIL
	// Every copy shares the draw's light slots, which live on instance 0.
	writeLightIndices(0);
#else
	writeLightIndices();
#endif

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
