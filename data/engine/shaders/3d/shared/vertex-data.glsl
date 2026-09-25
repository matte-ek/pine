// The block the vertex stage writes and the fragment stage reads, and the vertex-side helpers that
// fill the parts of it every lit shader fills the same way.
//
// Shared rather than copied because the light slots are the fiddliest thing in here: the
// literal-subscript rule below is a driver workaround that is invisible when it is violated (the
// surface simply loses its dynamic lights), so it is kept in one place rather than in one copy per
// shader that would have to be remembered separately.
//
// Include it after shared/common.glsl, which declares the Light and Instance structs and the
// uniform buffers the helpers read. The including file defines PINE_VERTEX_STAGE before the include
// to get the writable 'out' block and the helpers; every other stage gets the matching 'in' block.

#ifdef PINE_VERTEX_STAGE

out VertexData
{
	vec2 uv;
	vec3 worldPosition;
	vec3 cameraPos;
	vec3 cameraDir;
	vec3 normalDir;

	// Always world space, unlike normalDir which is rotated into tangent space for normal-mapped
	// materials. Shadow lookups need a normal they can offset a world position along, so they
	// cannot use normalDir.
	vec3 worldNormal;

	vec3 lightDir[8];
	flat int lightIndices[8];
}vOut;

// Copies the light slots of one entry of the Instances block into the varyings.
void writeLightIndices(int instanceIndex)
{
	vOut.lightIndices[0] = instances[instanceIndex].lightIndices[0].x;
	vOut.lightIndices[1] = instances[instanceIndex].lightIndices[0].y;
	vOut.lightIndices[2] = instances[instanceIndex].lightIndices[0].z;
	vOut.lightIndices[3] = instances[instanceIndex].lightIndices[0].w;

	vOut.lightIndices[4] = instances[instanceIndex].lightIndices[1].x;
	vOut.lightIndices[5] = instances[instanceIndex].lightIndices[1].y;
	vOut.lightIndices[6] = instances[instanceIndex].lightIndices[1].z;
	vOut.lightIndices[7] = instances[instanceIndex].lightIndices[1].w;
}

// The light slots of the instance being drawn.
void writeLightIndices()
{
	writeLightIndices(gl_InstanceID);
}

// Pass everything directly in world space.
// lightDir[0] is the directional light; lightDir[n] (n = 1..7) is the direction towards the light in
// instance light slot n - 1 (slots 0-4 point lights, slots 5-6 spot lights). Written with literal
// subscripts: dynamically indexing this varying array misbehaves on some drivers (NVIDIA).
// lightDir[] is sized 8, so slot 6 is the last one that fits without growing it.
//
// Needs vOut.worldPosition and vOut.lightIndices, so call it after both have been written.
void writeLightDirections()
{
	vOut.lightDir[0] = normalize(lights[0].directionToLight);
	vOut.lightDir[1] = normalize(lights[vOut.lightIndices[0]].position - vOut.worldPosition.xyz);
	vOut.lightDir[2] = normalize(lights[vOut.lightIndices[1]].position - vOut.worldPosition.xyz);
	vOut.lightDir[3] = normalize(lights[vOut.lightIndices[2]].position - vOut.worldPosition.xyz);
	vOut.lightDir[4] = normalize(lights[vOut.lightIndices[3]].position - vOut.worldPosition.xyz);
	vOut.lightDir[5] = normalize(lights[vOut.lightIndices[4]].position - vOut.worldPosition.xyz);
	vOut.lightDir[6] = normalize(lights[vOut.lightIndices[5]].position - vOut.worldPosition.xyz);
	vOut.lightDir[7] = normalize(lights[vOut.lightIndices[6]].position - vOut.worldPosition.xyz);
}

// Rotates the directions writeLightDirections() just wrote out of world space and into the tangent
// space a normal-mapped surface shades in. Same literal-subscript rule as above.
void transformLightDirections(mat3 tangentMatrix)
{
	vOut.lightDir[0] = tangentMatrix * vOut.lightDir[0];
	vOut.lightDir[1] = tangentMatrix * vOut.lightDir[1];
	vOut.lightDir[2] = tangentMatrix * vOut.lightDir[2];
	vOut.lightDir[3] = tangentMatrix * vOut.lightDir[3];
	vOut.lightDir[4] = tangentMatrix * vOut.lightDir[4];
	vOut.lightDir[5] = tangentMatrix * vOut.lightDir[5];
	vOut.lightDir[6] = tangentMatrix * vOut.lightDir[6];
	vOut.lightDir[7] = tangentMatrix * vOut.lightDir[7];
}

// The basis that takes a world-space vector into the tangent space of a surface with this normal
// and this tangent. Both are expected to be in world space already.
mat3 CreateTangentMatrix(vec3 worldNormal, vec3 worldTangent)
{
	vec3 worldBiTangent = normalize(cross(worldNormal, worldTangent));

	return mat3(
		worldTangent.x, worldBiTangent.x, worldNormal.x,
		worldTangent.y, worldBiTangent.y, worldNormal.y,
		worldTangent.z, worldBiTangent.z, worldNormal.z
	);
}

#else

in VertexData
{
	vec2 uv;
	vec3 worldPosition;
	vec3 cameraPos;
	vec3 cameraDir;
	vec3 normalDir;
	vec3 worldNormal;
	vec3 lightDir[8];
	flat int lightIndices[8];
}vIn;

// The world normal of the face actually being shaded.
//
// vIn.worldNormal is the normal the geometry was authored with, which points the wrong way on the
// far side of a surface drawn with both of its faces (MaterialRenderFace::Both - a leaf card, a
// sheet of grass). Shadow lookups offset their sample position along this normal, so handing them
// the authored one on such a face pushes the sample *into* the surface and it shadows itself.
//
// For anything drawn with a face culled this is vIn.worldNormal unchanged: the culled face never
// reaches the fragment stage, so gl_FrontFacing is always true there.
//
// A shader whose normal belongs to neither face defines PINE_FACE_INDEPENDENT_NORMAL before
// including this file, and gets vIn.worldNormal on both. Terrain detail does: it shades with the
// ground's normal, which flipping would point into the ground.
vec3 FacingWorldNormal()
{
#ifdef PINE_FACE_INDEPENDENT_NORMAL
	return vIn.worldNormal;
#else
	return gl_FrontFacing ? vIn.worldNormal : -vIn.worldNormal;
#endif
}

#endif
