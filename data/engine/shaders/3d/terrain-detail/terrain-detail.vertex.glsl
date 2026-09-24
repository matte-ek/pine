#version 430 core

#define PINE_VERTEX_STAGE

layout(location = 0) in vec3 vertex;
layout(location = 1) in vec3 normal;
layout(location = 2) in vec2 uv;
layout(location = 3) in vec3 tangent;

#include "shared/common.glsl"
#include "shared/vertex-data.glsl"

uniform bool hasTangentData;

// Thousands of copies of one mesh in a single draw, each placed by an entry in a storage buffer
// that is written once and kept, rather than by the Instances block, which is rewritten for every
// draw and holds MAX_INSTANCE_COUNT at most. instances[0] still carries the terrain's transform and
// the light slots the copies share. Needs GLSL 4.30 for the storage block.

// Mirrors ShaderStorages::TerrainDetailInstanceData.
struct TerrainDetailInstance
{
	// xyz the terrain-local position, w the uniform scale.
	vec4 positionScale;

	// x = cos(yaw), y = sin(yaw). zw = the ground normal's x and z, see GetGroundNormal.
	vec4 orientation;
};

layout(std430, binding = TERRAIN_DETAIL_INSTANCE_BINDING) readonly buffer TerrainDetailInstances
{
	TerrainDetailInstance detailInstances[];
};

// x = the camera distance at which a copy starts shrinking, y = the one at which it is gone.
uniform vec2 detailFade;

// How far a copy's shading normal leans from its mesh's own normal towards the ground's. Grass
// cards face every way, so shaded by their own normals the ones turned from a light go black next
// to lit ground; leaning them onto the ground's normal lights them the way the ground around them
// is lit, and the rest keeps a little of the model's shape. Above 0.5, so that a mesh normal
// pointing straight into the ground still leaves a normal pointing out of it.
const float GroundNormalWeight = 0.8;

// This copy's placement within the terrain, shrunk towards nothing as it nears the draw distance
// so the edge of the detail sinks into the ground instead of popping.
mat4 GetDetailPlacement(TerrainDetailInstance detail, mat4 terrainTransform, vec3 cameraPosition)
{
	vec3 worldOrigin = (terrainTransform * vec4(detail.positionScale.xyz, 1.0)).xyz;
	float fade = 1.0 - smoothstep(detailFade.x, detailFade.y, length(worldOrigin - cameraPosition));

	float scale = detail.positionScale.w * fade;
	float cosYaw = detail.orientation.x;
	float sinYaw = detail.orientation.y;

	// Columns: a rotation about +y, scaled, then the translation.
	return mat4(
		vec4(cosYaw * scale, 0.0, -sinYaw * scale, 0.0),
		vec4(0.0, scale, 0.0, 0.0),
		vec4(sinYaw * scale, 0.0, cosYaw * scale, 0.0),
		vec4(detail.positionScale.xyz, 1.0));
}

// The ground's normal where this copy stands. Only x and z are stored: ground always faces up, so
// y is the positive rest of a unit vector. Terrain is never rotated, so this is also world space.
vec3 GetGroundNormal(TerrainDetailInstance detail)
{
	vec2 xz = detail.orientation.zw;

	return vec3(xz.x, sqrt(max(1.0 - dot(xz, xz), 0.0)), xz.y);
}

void main()
{
	vec4 vertexPosition = vec4(vertex, 1.0);

	mat4 terrainTransform = instances[0].transformationMatrix;
	vec3 cameraPosition = (inverse(viewMatrix) * vec4(0.0, 0.0, 0.0, 1.0)).xyz;

	TerrainDetailInstance detail = detailInstances[gl_InstanceID];

	mat4 transformationMatrix = terrainTransform * GetDetailPlacement(detail, terrainTransform, cameraPosition);

	vOut.worldPosition = (transformationMatrix * vertexPosition).xyz;
	vOut.uv = uv;
	vOut.cameraPos = cameraPosition;
	vOut.cameraDistance = length(vOut.worldPosition - vOut.cameraPos);

	// Every copy shares the draw's light slots, which live on instance 0.
	writeLightIndices(0);

	vec3 meshNormal = normalize((transformationMatrix * vec4(normal, 0.0)).xyz);
	vec3 worldNormalDir = normalize(mix(meshNormal, GetGroundNormal(detail), GroundNormalWeight));

	vOut.worldNormal = worldNormalDir;

	vec3 cameraDir = normalize(vOut.cameraPos - vOut.worldPosition.xyz);

	writeLightDirections();

	// Same as the generic shader: shade in tangent space only when the material has a normal map.
	if (hasTangentData)
	{
		vec3 meshTangent = (transformationMatrix * vec4(tangent, 0.0)).xyz;

		// Made perpendicular to the leaned normal again, which the mesh's own tangent no longer is.
		vec3 worldTangent = normalize(meshTangent - dot(meshTangent, worldNormalDir) * worldNormalDir);

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
}
