#include "shared/lightning/shadows.glsl"

BaseLightResult CalculateBaseLightning(Surface surface)
{
    BaseLightResult result;

#if defined(OVERRIDE_MAT_COLORS)
    vec3 diffuseColor = override.diffuseColor;
#else
    vec3 diffuseColor = surface.diffuseColor;
#endif

    float diffuseFactor = max(dot(surface.normal, surface.lightDirection), 0.0);

    // We get more accurate specular results if we use a vector directed half-way
    // to our camera from the light.
    vec3 halfwayDirection = normalize(surface.lightDirection + vIn.cameraDir);
    float specularFactor = pow(max(dot(surface.normal, halfwayDirection), 0.0), surface.shininess);

    vec3 ambient    = (surface.ambientColor + (world.ambientColor.rgb * diffuseColor));
    vec3 diffuse    = surface.lightColor * diffuseColor * diffuseFactor;
    vec3 specular   = surface.lightColor * surface.specularColor * specularFactor;

    result.diffuse = diffuse;
    result.specular = specular;
    result.ambient = max(1 - diffuseFactor, 0) * ambient;

    return result;
}

vec3 CalculateDirectionalLight(Surface surface)
{
    surface.lightDirection = vIn.lightDir[0];
    surface.lightColor = lights[0].color;

    BaseLightResult result = CalculateBaseLightning(surface);

    result.diffuse *= min(ComputeShadowFactor(), 1);

    return result.diffuse + result.specular + result.ambient;
}

vec3 CalculateSpotLight(Surface surface, int index, int directionIndex)
{
    surface.lightDirection = vIn.lightDir[directionIndex];
    surface.lightColor = lights[index].color;

    BaseLightResult baseResult = CalculateBaseLightning(surface);

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

vec3 CalculatePointLight(Surface surface, int index, int directionIndex)
{
    surface.lightDirection = vIn.lightDir[directionIndex];
    surface.lightColor = lights[index].color;

    BaseLightResult baseResult = CalculateBaseLightning(surface);

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
