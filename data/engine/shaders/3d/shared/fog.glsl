// Exponential height fog, applied by every lit shader as the last step of its fragment stage and by
// the skybox. Both use the same model, so geometry and sky fade to the same colour at the horizon.
// Include it after shared/common.glsl, which declares the World block.
//
// The fog's density is world.fogSettings.x at the height world.fogSettings.z, and thins by a
// factor of e every 1 / world.fogSettings.y units above it (a falloff of 0 is the same fog at every
// height). A density of 0 turns fog off. How much of the light along a ray the fog replaces is
// 1 - exp(-opticalDepth), where the optical depth is the density integrated along the ray.

// The density at the camera, where every ray starts. The exponent is clamped so a camera far below
// the fog's base height cannot overflow it.
float FogDensityAtCamera(vec3 cameraPosition)
{
    float heightAboveBase = cameraPosition.y - world.fogSettings.z;

    return world.fogSettings.x * exp(clamp(-world.fogSettings.y * heightAboveBase, -80.0, 80.0));
}

vec3 ApplyFog(vec3 color, float opticalDepth)
{
    return mix(color, world.fogColor.rgb, 1.0 - exp(-opticalDepth));
}

// Fog over a surface at surfacePosition.
vec3 ApplySurfaceFog(vec3 color, vec3 cameraPosition, vec3 surfacePosition)
{
    if (world.fogSettings.x <= 0.0)
    {
        return color;
    }

    vec3 ray = surfacePosition - cameraPosition;

    // Integrated along the ray, the density is the camera's density times the ray's length times
    // (1 - e^-h) / h, where h is how many falloff lengths the ray climbs. That ratio is 0 / 0 on a
    // level ray, so near there its first-order series stands in.
    float climb = clamp(world.fogSettings.y * ray.y, -80.0, 80.0);
    float heightScale = abs(climb) > 0.001 ? (1.0 - exp(-climb)) / climb : 1.0 - 0.5 * climb;

    float opticalDepth = FogDensityAtCamera(cameraPosition) * length(ray) * heightScale;

    return ApplyFog(color, opticalDepth);
}

// Fog over the sky, which is infinitely far away along viewDirection. A rising ray climbs out of
// the fog, so the same integral ends at a finite depth. A level or falling ray never does, and
// neither does any ray through fog with no falloff, so the sky there is the fog colour.
vec3 ApplySkyFog(vec3 color, vec3 cameraPosition, vec3 viewDirection)
{
    if (world.fogSettings.x <= 0.0)
    {
        return color;
    }

    float climbRate = world.fogSettings.y * normalize(viewDirection).y;

    if (climbRate <= 0.0)
    {
        return world.fogColor.rgb;
    }

    float opticalDepth = FogDensityAtCamera(cameraPosition) / climbRate;

    return ApplyFog(color, opticalDepth);
}
