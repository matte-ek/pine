#include "shared/lightning/shadows.glsl"

// The diffuse color to shade with, honouring the material-color override.
vec3 GetSurfaceDiffuseColor(Surface surface)
{
#if defined(OVERRIDE_MAT_COLORS)
    return override.diffuseColor;
#else
    return surface.diffuseColor;
#endif
}

// Light that is not attributable to any one light source. Computed once per fragment, on purpose:
// this used to be bundled into every light's result and summed, which meant scene brightness scaled
// with how many lights happened to reach an object (and changed as light slots churned while
// moving), a light contributed full ambient no matter how distant, and a spot light lit the whole
// sphere around it because neither attenuation nor the cone mask ever touched the ambient term.
//
// This is the seam an actual environment term slots into: skybox-derived irradiance, even a cheap
// hemisphere or SH approximation, replaces the body here without any light function changing.
vec3 CalculateAmbientLight(Surface surface)
{
    return surface.ambientColor + world.ambientColor.rgb * GetSurfaceDiffuseColor(surface);
}

BaseLightResult CalculateBaseLightning(Surface surface)
{
    BaseLightResult result;

    vec3 diffuseColor = GetSurfaceDiffuseColor(surface);

    float diffuseFactor = max(dot(surface.normal, surface.lightDirection), 0.0);

    // We get more accurate specular results if we use a vector directed half-way
    // to our camera from the light.
    vec3 halfwayDirection = normalize(surface.lightDirection + vIn.cameraDir);
    float specularFactor = pow(max(dot(surface.normal, halfwayDirection), 0.0), surface.shininess);

    result.diffuse = surface.lightColor * diffuseColor * diffuseFactor;
    result.specular = surface.lightColor * surface.specularColor * specularFactor;

    return result;
}

vec3 CalculateDirectionalLight(Surface surface)
{
    surface.lightDirection = vIn.lightDir[0];
    surface.lightColor = lights[0].color;

    BaseLightResult result = CalculateBaseLightning(surface);

    result.diffuse *= min(ComputeShadowFactor(), 1);

    return result.diffuse + result.specular;
}

// Contribution of a light that has a position, with distance attenuation applied. Point and spot
// lights are both this; the spot adds a cone mask on top, so keep the shared parts here rather
// than growing a second copy that has to be kept in sync by hand.
//
// lightDirection is the per-vertex direction towards the light (vIn.lightDir[n]). Callers pass the
// vector itself rather than an index: dynamically indexing the vIn.lightDir[] varying array returns
// garbage on some drivers (seen on NVIDIA), which silently zeroes N.L so the light contributes
// nothing and the surface falls back to the flat ambient term alone.
BaseLightResult CalculatePositionalLight(Surface surface, int index, vec3 lightDirection)
{
    surface.lightDirection = lightDirection;
    surface.lightColor = lights[index].color;

    BaseLightResult result = CalculateBaseLightning(surface);

    float lightDistance = length(lights[index].position - vIn.worldPosition);

    // x component being the constant factor, y is the linear factor and z the quadratic factor
    vec3 attenuationFactors = lights[index].attenuation;

    float attenuation = 1.0 / (attenuationFactors.x +
                               attenuationFactors.y * lightDistance +
                               attenuationFactors.z * (lightDistance * lightDistance));

    result.diffuse *= attenuation;
    result.specular *= attenuation;

    return result;
}

vec3 CalculatePointLight(Surface surface, int index, vec3 lightDirection)
{
    BaseLightResult result = CalculatePositionalLight(surface, index, lightDirection);

    return result.diffuse + result.specular;
}

vec3 CalculateSpotLight(Surface surface, int index, vec3 lightDirection)
{
    BaseLightResult result = CalculatePositionalLight(surface, index, lightDirection);

    // The cone test is done in world space on purpose. 'lightDirection' has been rotated into
    // tangent space for normal-mapped materials (see generic.vertex.glsl), while the light's
    // directionToLight is always world space - dotting the two mixes coordinate spaces and makes
    // the cone edge depend on each triangle's tangent frame. vIn.worldPosition is already a
    // varying, so recomputing here costs one subtract + normalize and no extra interpolators.
    vec3 worldToLight = normalize(lights[index].position - vIn.worldPosition);

    // Both vectors point *towards* the light (see the convention in common.glsl), so they agree
    // at +1 on the cone axis and fall off from there. Negating either one inverts the cone and
    // lights the hemisphere behind the lamp instead.
    float coneAxis = dot(worldToLight, lights[index].directionToLight);

    // cutOffOuter < cutOffInner is guaranteed on upload (Renderer3D::AddLight), so smoothstep
    // never sees equal or reversed edges.
    float cone = smoothstep(lights[index].cutOffOuter, lights[index].cutOffInner, coneAxis);

    result.diffuse *= cone;
    result.specular *= cone;

    return result.diffuse + result.specular;
}
