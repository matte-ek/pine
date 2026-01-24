#version 420 core

layout(location = 0) out vec4 m_OutputColor;

// NOTE: These needs to be added onto the `.shader` file as well!
struct TextureSamplers
{
    sampler2D diffuse;  
    sampler2D specular;  
    sampler2D normal;
    sampler2DArrayShadow shadowMap;
};

struct BaseResult
{
    vec3 diffuse;
    vec3 specular;
    vec3 ambient;
};

#include "shared/common.glsl"

in VertexData
{
	vec2 uv;
	vec3 worldPosition;
    vec3 cameraPos;
	vec3 cameraDir;
	vec3 normalDir;
	vec3 lightDir[8];
    flat int lightIndices[8];
}vIn;

uniform TextureSamplers textureSamplers;

uniform bool hasDirectionalShadowMap;
uniform bool hasTangentData;

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

    vec3 ambient = (material.ambientColor + (worldAmbientColor * diffuseColor)) * texture(textureSamplers.diffuse, vIn.uv * material.uvScale).xyz;
    vec3 diffuse = lights[lightIndex].color * diffuseColor * texture(textureSamplers.diffuse, vIn.uv * material.uvScale).xyz * diffuseFactor;
    vec3 specular = lights[lightIndex].color * material.specularColor * (1 - texture(textureSamplers.specular, vIn.uv * material.uvScale).xyz) * specularFactor;

    result.diffuse = diffuse;
    result.specular = specular;
    //result.ambient = max(0.5 - diffuseFactor, 0) * ambient;
    result.ambient = ambient;

    return result;
}

vec3 calculateDirectionalLight()
{
    BaseResult result = calculateBaseLightning(vIn.lightDir[0], 0);

    if (hasDirectionalShadowMap)
    {
        float fragCameraDistance = length(vIn.worldPosition - vIn.cameraPos);
        
        float th1 = 10.f;
        //float th2 = 30.f;
        //float th3 = 30.f;

        int cascadeIndex = int(fragCameraDistance > th1);// + int(fragCameraDistance > th2);// + int(fragCameraDistance > th3);
        mat4 lsMatrix = lightSpaceMatrix[cascadeIndex];

        vec4 pointInDepthMap = lsMatrix * vec4(vIn.worldPosition, 1.0);
        vec3 pointNormalized = pointInDepthMap.xyz / pointInDepthMap.w;

       // bool visible = all(greaterThanEqual(pointNormalized, vec3(-1.0))) &&
       //                all(lessThanEqual(pointNormalized, vec3(1.0)));

        pointNormalized = pointNormalized * 0.5 + 0.5;

//        float closestDepth = ;
        vec2 samplePoint = pointNormalized.xy;
        float currentDepth = pointNormalized.z;
        float shadow = 0.0; //mix(0.45, 1.0, step(currentDepth - closestDepth, 0.002));
        float texelSize = 1.0 / textureSize(textureSamplers.shadowMap, 0).x;

        for (int x = -1; x <= 1;x++)
        {
            for (int y = -1;y <= 1;y++)
            {
                shadow += texture(textureSamplers.shadowMap, 
                    vec4(samplePoint.x + x * texelSize, samplePoint.y + y * texelSize, cascadeIndex, currentDepth)).x;

                // Manual PCF:
                // shadow += mix(0.45, 1.0, step(currentDepth - depth, 0.002));
            }
        }

        shadow /= 9.0;
        shadow = min(shadow, 1);
        shadow = max(shadow, 0.4);

        result.diffuse *= shadow;

/*
        if (cascadeIndex == 0) 
            result.diffuse *= vec3(0, 1, 0);
        else if (cascadeIndex == 1)
            result.diffuse *= vec3(1, 1, 0);
        else if (cascadeIndex == 2)
            result.diffuse *= vec3(1, 0, 0);
        else if (cascadeIndex == 3)
            result.diffuse *= vec3(0, 0, 1);
*/

    }

    return result.diffuse + result.specular + result.ambient;
}

vec3 calculateSpotLight(int index, int directionIndex)
{
    BaseResult baseResult = calculateBaseLightning(vIn.lightDir[directionIndex], index);

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

vec3 calculatePointLight(int index, int directionIndex)
{
    BaseResult baseResult = calculateBaseLightning(vIn.lightDir[directionIndex], index);

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
    vec3 lightColorOutput = vec3(0.f);

    for (int i = 0; i < 5;i++) {
        if (vIn.lightIndices[i] != 0) {
            lightColorOutput += calculateSpotLight(vIn.lightIndices[i], i + 1);
        }
    }

    return lightColorOutput;
}

vec3 calculatePointLights()
{
    vec3 a = vec3(0.f);

    if (vIn.lightIndices[5] != 0) {
        a = calculatePointLight(vIn.lightIndices[5], 6);
    }

    return a;
}

void main(void)
{
    #shader preFragment

#ifdef VERSION_DISCARD
    vec4 frag = texture(textureSamplers.diffuse, vIn.uv * material.uvScale);

    if (frag.w < 0.001f || frag.r + frag.g + frag.b <= 0.5f)
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