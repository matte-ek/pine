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

const float TwoPi = 6.28318530718;

// How far one gust is from the next along the wind, in world units.
const float GustLength = 12.0;

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

// How far the wind pushes a point of this copy, from world.wind (see Renderer3D::SceneWind). Gusts
// roll across the terrain along the wind, so neighbouring copies lean together without moving in
// lockstep, and a smaller flutter gives each copy some motion of its own. The lean grows with
// height, so the base stays planted, and never exceeds the wind's strength times that height:
// TerrainDetail's culling (GetDetailReach) relies on that bound.
vec3 GetWindOffset(vec3 worldOrigin, float heightAboveBase)
{
	vec2 direction = world.wind.xy;
	float strength = world.wind.z;
	float phase = world.wind.w;

	float distanceAlongWind = dot(worldOrigin.xz, direction);
	float gust = 0.5 + 0.5 * sin(phase - distanceAlongWind * (TwoPi / GustLength));

	// At three times the gust rate, so it also repeats whole cycles within the phase's wrap.
	float flutter = sin(3.0 * phase + worldOrigin.x * 1.7 + worldOrigin.z * 2.3);

	// Weights summing to 1, so the lean stays within [-strength, strength].
	float lean = strength * (0.85 * gust + 0.15 * flutter);

	return vec3(direction.x, 0.0, direction.y) * lean * heightAboveBase;
}

void main()
{
	vec4 vertexPosition = vec4(vertex, 1.0);

	mat4 terrainTransform = instances[0].transformationMatrix;
	vec3 cameraPosition = (inverse(viewMatrix) * vec4(0.0, 0.0, 0.0, 1.0)).xyz;

	TerrainDetailInstance detail = detailInstances[gl_InstanceID];

	mat4 transformationMatrix = terrainTransform * GetDetailPlacement(detail, terrainTransform, cameraPosition);

	vec3 worldOrigin = transformationMatrix[3].xyz;

	vOut.worldPosition = (transformationMatrix * vertexPosition).xyz;
	vOut.worldPosition += GetWindOffset(worldOrigin, max(vOut.worldPosition.y - worldOrigin.y, 0.0));

	vOut.uv = uv;
	vOut.cameraPos = cameraPosition;

	// Every copy shares the draw's light slots, which live on instance 0.
	writeInstanceLighting(0);

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

	// From the swayed position. Detail is not in the depth pre-pass, so nothing else has to match it.
	gl_Position = projectionMatrix * viewMatrix * vec4(vOut.worldPosition, 1.0);
}
