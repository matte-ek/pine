#version 420 core

layout(location = 0) out vec4 m_OutputColor;

struct TextureSamplers
{
    sampler2D diffuse;  
    sampler2D specular;  
    sampler2D normal;  
};

struct Light 
{
	vec3 position;
	vec3 rotation;
	vec3 color;
	vec3 attenuation;
	float cutOffAngle;
	float cutOffSmoothness;
};

layout(std140) uniform Matrices
{
	mat4 projectionMatrix;
	mat4 viewMatrix;
};

layout(std140) uniform Lights 
{
	Light lights[32];
};

layout(std140) uniform Material
{
    vec3 diffuseColor;
    vec3 specularColor;
    vec3 ambientColor;
    float shininess;
    float uvScale;
}material;

in VertexData
{
	vec2 uv;
	vec3 worldPosition;
	vec3 cameraDir;
	vec3 normalDir;
	vec3 lightDir[4];
}input;

uniform TextureSamplers textureSamplers;
uniform bool hasTangentData;

// Light indices from Lights that should affect this object
uniform ivec4 lightsIndices;

vec3 calculateBaseLightning(vec3 lightDirection, int lightIndex)
{
    // Calculate normal dir and account for tangent data
    vec3 normal;

    if (hasTangentData)
        normal = normalize((2.0 * texture(textureSamplers.normal, input.uv * material.uvScale)).xyz);
    else
        normal = input.normalDir;

    float diffuseFactor = max(dot(normal, lightDirection), 0.0);

    // We get more accurate specular results if we use a vector directed half-way
    // to our camera from the light.
    vec3 halfwayDirection = normalize(lightDirection + input.cameraDir);
    float specularFactor = pow(max(dot(normal, halfwayDirection), 0.0), material.shininess);

    vec3 ambient = lights[lightIndex].color * material.diffuseColor * material.ambientColor * texture(textureSamplers.diffuse, input.uv * material.uvScale).xyz;
    vec3 diffuse = lights[lightIndex].color * material.diffuseColor * texture(textureSamplers.diffuse, input.uv * material.uvScale).xyz * diffuseFactor;
    vec3 specular = lights[lightIndex].color * material.specularColor * texture(textureSamplers.specular, input.uv * material.uvScale).xyz * specularFactor;

    return diffuse + specular + ambient;
}

vec3 calculateDirectionalLight()
{
    return calculateBaseLightning(input.lightDir[0], 0);
}

void main(void)
{
    vec4 directionalLight = vec4(calculateDirectionalLight(), 1.0);

    m_OutputColor = directionalLight;
}