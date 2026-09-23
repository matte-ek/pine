// Distance fog, applied by every lit shader as the last step of its fragment stage. Include it
// after shared/common.glsl and shared/vertex-data.glsl, which declare the World block and vIn.

// Classic linear fog: blends towards world.fogColor from the camera out to the view distance.
// world.fogSettings.x is the view distance, .y the intensity (0 disables it).
vec3 ApplyDistanceFog(vec3 color)
{
    if (world.fogSettings.y <= 0.0)
    {
        return color;
    }

    float fogFactor = clamp(vIn.cameraDistance / max(world.fogSettings.x, 0.001), 0.0, 1.0) * world.fogSettings.y;

    return mix(color, world.fogColor.rgb, fogFactor);
}
