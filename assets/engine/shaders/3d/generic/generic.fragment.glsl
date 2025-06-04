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

struct BaseResult
{
    vec3 diffuse;
    vec3 specular;
    vec3 ambient;
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
    vec3 cameraPos;
	vec3 cameraDir;
	vec3 normalDir;
	vec3 lightDir[4];
}vIn;

uniform TextureSamplers textureSamplers;

uniform bool hasDirectionalShadowMap;

uniform bool hasTangentData;

// Light indices from Lights that should affect this object
uniform ivec4 lightIndices;

#shader hooks

BaseResult calculateBaseLightning(vec3 lightDirection, int lightIndex)
{
    BaseResult result;

#if defined(OVERRIDE_MAT_COLORS)
    vec3 diffuseColor = override.diffuseColor;
#else
    vec3 diffuseColor = material.diffuseColor;
#endif

    // Calculate normal dir and account for tangent data
    vec3 normal;

    if (hasTangentData)
        normal = normalize((2.0 * texture(textureSamplers.normal, vIn.uv * material.uvScale) - 1.0).xyz);
    else
        normal = vIn.normalDir;

    float diffuseFactor = max(dot(normal, lightDirection), 0.0);

    // We get more accurate specular results if we use a vector directed half-way
    // to our camera from the light.
    vec3 halfwayDirection = normalize(lightDirection + vIn.cameraDir);
    float specularFactor = pow(max(dot(normal, halfwayDirection), 0.0), material.shininess);

    vec3 ambient = (material.ambientColor + vec3(0)) * texture(textureSamplers.diffuse, vIn.uv * material.uvScale).xyz;
    vec3 diffuse = lights[lightIndex].color * diffuseColor * texture(textureSamplers.diffuse, vIn.uv * material.uvScale).xyz * diffuseFactor;
    vec3 specular = lights[lightIndex].color * material.specularColor * (1 - texture(textureSamplers.specular, vIn.uv * material.uvScale).xyz) * specularFactor;

    result.diffuse = diffuse;
    result.specular = specular;
    result.ambient = (1.f - diffuseFactor) * ambient;

    return result;
}

vec3 calculateDirectionalLight()
{
    BaseResult result = calculateBaseLightning(vIn.lightDir[0], 0);

    return result.diffuse + result.specular + result.ambient;
}

vec3 calculateSpotLight(int index)
{
    BaseResult baseResult = calculateBaseLightning(vIn.lightDir[index], index);

    float lightDistance = length(lights[index].position - vIn.worldPosition);
    
    // x component being the constant factor, y is the linear factor and z the quadratic factor
    vec3 attenuationFactors = lights[index].attenuation;

    float attenuation = 1.0 / (attenuationFactors.x +
                               attenuationFactors.y * lightDistance + 
                               attenuationFactors.z * (lightDistance * lightDistance));

    baseResult.diffuse *= attenuation;
    baseResult.specular *= attenuation;

    return baseResult.diffuse + baseResult.specular + baseResult.ambient;
}

vec3 calculatePointLight(int index)
{
    BaseResult baseResult = calculateBaseLightning(vIn.lightDir[index], index);

    float lightDistance = length(lights[index].position - vIn.worldPosition);
    
    // x component being the constant factor, y is the linear factor and z the quadratic factor
    vec3 attenuationFactors = lights[index].attenuation;

    float attenuation = 1.0 / (attenuationFactors.x +
                               attenuationFactors.y * lightDistance + 
                               attenuationFactors.z * (lightDistance * lightDistance));

    baseResult.diffuse *= attenuation;
    baseResult.specular *= attenuation;

    float lightRotationDirection = dot(vIn.lightDir[index], -lights[index].rotation);
    float lightRotationDirectionStep = smoothstep(lights[index].cutOffAngle, lights[index].cutOffSmoothness, lightRotationDirection);

    baseResult.diffuse *= lightRotationDirectionStep;
    baseResult.specular *= lightRotationDirectionStep;

    return baseResult.diffuse + baseResult.specular + baseResult.ambient;
}

vec3 calculateSpotLights()
{
    vec3 a = vec3(0.f);
    vec3 b = vec3(0.f);

    if (lightIndices.x != 0) {
        a = calculateSpotLight(lightIndices.x);
    }

    if (lightIndices.y != 0) {
        b = calculateSpotLight(lightIndices.y);
    }

    return a + b;
}

vec3 calculatePointLights()
{
    vec3 a = vec3(0.f);
    vec3 b = vec3(0.f);

    if (lightIndices.z != 0) {
        a = calculatePointLight(lightIndices.z);
    }

    if (lightIndices.w != 0) {
        b = calculatePointLight(lightIndices.w);
    }

    return a + b;
}

void main(void)
{
    #shader preFragment

#ifdef VERSION_DISCARD
    if (texture(textureSamplers.diffuse, vIn.uv * material.uvScale).w < 0.001f)
    {
        discard;
    }
#endif

    vec4 directionalLight = vec4(calculateDirectionalLight(), 1.0);
    vec4 spotLights = vec4(calculateSpotLights(), 1.0);
    vec4 pointLights = vec4(calculatePointLights(), 1.0);

    m_OutputColor = directionalLight + spotLights + pointLights;

    #shader postFragment
}