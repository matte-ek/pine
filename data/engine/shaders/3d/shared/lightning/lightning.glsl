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

    // Windowed inverse-square falloff. The 1/(1+d^2) term is the physical part; the window is what
    // drives it to exactly zero at 'range' instead of merely small.
    //
    // Reaching zero is not a tidiness point, it is a correctness one: a light's shadow map only
    // covers out to its range, so any illumination surviving past that distance is illumination the
    // shadow pass never rendered casters for, and it lights geometry that should be occluded. The
    // old constant/linear/quadratic curve had no zero at all - with the shipped defaults it stayed
    // above 1/256 until ~181 units - which is why lights had to be treated as infinite.
    vec3 toLight = lights[index].position - vIn.worldPosition;
    float distanceSqr = dot(toLight, toLight);

    float range = lights[index].range;
    float window = clamp(1.0 - distanceSqr / max(range * range, 0.0001), 0.0, 1.0);

    float attenuation = (window * window) / (1.0 + distanceSqr);

    result.diffuse *= attenuation;
    result.specular *= attenuation;

    return result;
}

vec3 CalculatePointLight(Surface surface, int index, vec3 lightDirection)
{
    BaseLightResult result = CalculatePositionalLight(surface, index, lightDirection);

    // Same call as the spot path below - SampleLocalShadow picks the cube face itself from the
    // light's view count, so nothing here has to know that this light spends six views and that one
    // spends one. Returns 1.0 for a light holding no tile, so an unshadowed point light costs one
    // compare.
    float shadow = SampleLocalShadow(index, vIn.worldPosition, FacingWorldNormal());

    return (result.diffuse + result.specular) * shadow;
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

    // Both terms, unlike the directional path which only shadows diffuse. A specular highlight
    // surviving inside a shadow reads as a light leak, and it is the more noticeable of the two.
    float shadow = SampleLocalShadow(index, vIn.worldPosition, FacingWorldNormal());

    result.diffuse *= shadow;
    result.specular *= shadow;

    return result.diffuse + result.specular;
}
