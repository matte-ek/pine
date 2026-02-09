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

#include "shared/uniform-buffers.glsl"