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
	vec3 attenuation;

	// Cosines of the cone half-angles, always cutOffOuter < cutOffInner (enforced on upload) so
	// they can be handed straight to smoothstep as (edge0, edge1).
	float cutOffOuter;
	float cutOffInner;
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