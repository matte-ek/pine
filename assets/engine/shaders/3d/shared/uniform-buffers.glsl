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
	vec3 blah;
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

layout(std140) uniform World
{
    vec4 ambientColor;
    vec4 fogColor;
    vec4 fogSettings;
}world;