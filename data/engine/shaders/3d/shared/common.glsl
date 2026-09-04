// All colors and textures reaching these shaders are already linear: authored colors are decoded
// CPU-side (SrgbToLinear in Math.hpp) and color textures use sRGB internal formats (hardware decode).
// The single linear->sRGB encode happens at the end of the post-process resolve pass.

struct Light
{
	vec3 position;

	// Unit vector pointing *towards* the light, i.e. the opposite of the direction the light
	// shines. This is the same convention as vIn.lightDir[], so the two can be compared directly.
	// Renderer3D::AddLight uploads -forward to make it hold. Negating this in a shader is what
	// used to invert the spot cone.
	vec3 directionToLight;

	vec3 color;
	float pad2;

	// World units at which this light's contribution reaches exactly zero. The falloff is windowed
	// against it, so it is a hard cutoff and not an asymptote - that is what lets a shadow far
	// plane sit exactly here without light leaking past where the shadow map ends.
	float range;
	float pad2a;
	float pad2b;

	// Cosines of the cone half-angles, always cutOffOuter < cutOffInner (enforced on upload) so
	// they can be handed straight to smoothstep as (edge0, edge1).
	float cutOffOuter;

	float cutOffInner;

	// Index of this light's first shadow view, -1 when it casts none. See LightsData::Light in
	// ShaderStorages.hpp for why it is -1 and not 0.
	int shadowViewIndex;

	// 1 for a spot, 6 for a point light's cube faces.
	int shadowViewCount;

	// Multiplier on the shadow term, faded while a light gains or loses its shadow tile.
	float shadowFade;
};

// Mirrors ShaderStorages::ShadowViewData::View. Every member is 16-byte aligned in std140, so
// unlike the Light struct this one needs no explicit padding to line up with the C++ side.
struct ShadowView
{
	mat4 viewProjection;

	// Where this view's tile lives in the atlas, in normalized atlas UV: xy = origin, zw = size.
	vec4 tileRect;

	// x = world texel size per unit distance from the view origin (0 for an ortho view),
	// y = normal offset in texels, z = shadow strength, w = unused.
	vec4 params;
};

struct Instance
{
	mat4 transformationMatrix;
	ivec4 lightIndices[2];
};

struct Surface
{
	vec3 diffuseColor;
	vec3 specularColor;
	vec3 ambientColor;
	vec3 normal;
	float shininess;

	vec3 lightDirection;
	vec3 lightColor;
};

// The contribution of a single light. Ambient is deliberately not in here: it belongs to the
// environment rather than to any one light, so it is computed once per fragment in main().
struct BaseLightResult
{
	vec3 diffuse;
    vec3 specular;
};

struct MaterialProperties
{
    vec3 diffuseColor;
    vec3 specularColor;
    vec3 ambientColor;
    float shininess;
    float uvScale;
};

struct MaterialSamplers
{
	sampler2D diffuse;  
    sampler2D specular;  
    sampler2D normal;
};

#include "shared/uniform-buffers.glsl"