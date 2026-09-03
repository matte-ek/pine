// All colors and textures reaching these shaders are already linear: authored colors are decoded
// CPU-side (SrgbToLinear in Math.hpp) and color textures use sRGB internal formats (hardware decode).
// The single linear->sRGB encode happens at the end of the post-process resolve pass.

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

struct BaseLightResult
{
	vec3 diffuse;
    vec3 specular;
    vec3 ambient;
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