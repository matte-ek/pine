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

layout(std140) uniform Material
{
    vec3 diffuseColor;
    vec3 specularColor;
    vec3 ambientColor;
    float shininess;
    float uvScale;
}material;

layout(std140) uniform Shadows
{
	mat4 lightSpaceMatrix[8];
};