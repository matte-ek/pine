#version 420 core

#define PINE_VERTEX_STAGE

layout(location = 0) in vec3 vertex;
layout(location = 1) in vec3 normal;
layout(location = 2) in vec2 uv;
layout(location = 3) in vec3 tangent;

#include "shared/common.glsl"
#include "shared/vertex-data.glsl"

void main()
{
	vec4 vertexPosition = vec4(vertex, 1.0);
	mat4 transformationMatrix = instances[gl_InstanceID].transformationMatrix;

	vOut.worldPosition = (transformationMatrix * vertexPosition).xyz;
	vOut.cameraPos = (inverse(viewMatrix) * vec4(0.0, 0.0, 0.0, 1.0)).xyz;

	// Terrain-local world units rather than a 0..1 span per chunk, so a layer's texture is
	// continuous across a chunk edge and its uv scale means "repeats per world unit". The fragment
	// stage derives the splat lookup from this same coordinate, so the two cannot drift apart.
	vOut.uv = uv;

	writeLightIndices();

	vec3 worldNormalDir = normalize((transformationMatrix * vec4(normal, 0.0)).xyz);

	vOut.worldNormal = worldNormalDir;

	vec3 cameraDir = normalize(vOut.cameraPos - vOut.worldPosition.xyz);

	writeLightDirections();

	// Unconditionally tangent space, unlike the generic shader's hasTangentData branch: a chunk
	// mesh always carries tangents, and a layer without a normal map is bound the flat default one
	// - which decodes to the surface normal and leaves the shading identical to not having one.
	// Branching per layer instead would mean four more uniforms for no visible difference.
	vec3 worldTangent = normalize((transformationMatrix * vec4(tangent, 0.0)).xyz);

	mat3 tangentMatrix = CreateTangentMatrix(worldNormalDir, worldTangent);

	transformLightDirections(tangentMatrix);

	vOut.cameraDir = tangentMatrix * cameraDir;
	vOut.normalDir = tangentMatrix * worldNormalDir;

	gl_Position = projectionMatrix * viewMatrix * transformationMatrix * vertexPosition;
}
