// Every shadow in the engine is sampled from here. sampler2DShadow, so a single texture() call is
// a hardware 2x2 PCF tap.
uniform sampler2DShadow ShadowAtlas;

#shader bind ShadowAtlas 17

// Samples one shadow view's tile and returns its shadow term, or 1.0 (fully lit) for anything the
// view cannot answer for.
//
// Applies no bias: the caller does, because no single offset suits both a cascade and a local
// light. A constant in projected depth especially does not, since perspective depth is not
// linear.
//
// 'pcfTaps' is the radius of a grid of hardware taps: 0 is a single 2x2 tap, 1 is a 3x3 grid.
//
// Indexing shadowViews[] dynamically is fine, since it is a uniform-block array. Only varying
// arrays such as vIn.lightDir[] need the hand-unrolled subscripts (NVIDIA).
float SampleShadowView(int viewIndex, vec3 samplePosition, int pcfTaps)
{
    ShadowView view = shadowViews[viewIndex];

    vec4 clipPosition = view.viewProjection * vec4(samplePosition, 1.0);

    // Behind the view's near plane - it projects to nonsense, so treat it as lit.
    if (clipPosition.w <= 0.0)
    {
        return 1.0;
    }

    vec3 projected = (clipPosition.xyz / clipPosition.w) * 0.5 + 0.5;

    // Outside this view, or past its far plane: no depth to compare against, so lit.
    if (projected.z > 1.0 ||
        any(lessThan(projected.xy, vec2(0.0))) || any(greaterThan(projected.xy, vec2(1.0))))
    {
        return 1.0;
    }

    // One atlas texel in tile UV: projected.xy spans one tile, so an atlas texel is
    // texelSize / tileRect.zw of it.
    vec2 texelSize = vec2(1.0) / vec2(textureSize(ShadowAtlas, 0));
    vec2 tileTexel = texelSize / view.tileRect.zw;

    // Half a texel of inset, so no tap in the footprint can cross the tile border.
    vec2 inset = tileTexel * 0.5;

    float shadow = 0.0;
    float taps = 0.0;

    for (int x = -pcfTaps; x <= pcfTaps; x++)
    {
        for (int y = -pcfTaps; y <= pcfTaps; y++)
        {
            vec2 tileUv = clamp(projected.xy + vec2(x, y) * tileTexel, inset, vec2(1.0) - inset);
            vec2 atlasUv = view.tileRect.xy + tileUv * view.tileRect.zw;

            shadow += texture(ShadowAtlas, vec3(atlasUv, projected.z));
            taps += 1.0;
        }
    }

    shadow /= taps;

    // params.z fades the shadow in and out as a light gains or loses its tile. Always 1.0 for a
    // cascade.
    return mix(1.0, shadow, view.params.z);
}

// Which of a point light's six views covers this direction: the major axis of L, in the order
// BuildPointView in Shadows.cpp builds the faces (+X, -X, +Y, -Y, +Z, -Z).
int SelectCubeFace(vec3 L)
{
    vec3 a = abs(L);

    if (a.x >= a.y && a.x >= a.z)
    {
        return L.x > 0.0 ? 0 : 1;
    }

    if (a.y >= a.z)
    {
        return L.y > 0.0 ? 2 : 3;
    }

    return L.z > 0.0 ? 4 : 5;
}

// The shadow term for one local light, or 1.0 when it is not casting.
float SampleLocalShadow(int lightIndex, vec3 worldPosition, vec3 worldNormal)
{
    int viewIndex = lights[lightIndex].shadowViewIndex;

    if (viewIndex < 0)
    {
        return 1.0;
    }

    vec3 toLight = lights[lightIndex].position - worldPosition;
    float lightDistance = length(toLight);
    vec3 lightDirection = toLight / max(lightDistance, 0.0001);

    // Normal offset: push the sample point off the surface along its normal before projecting.
    //
    // Sized in shadow-map texels, since acne is the depth drift across one texel's world footprint.
    // params.x is the world size of one texel per unit distance from the light, params.y the offset
    // in texels. Scaled by sin(angle between normal and light), a bounded stand-in for tan().
    //
    // Done before the face pick: offsetting after it can carry the sample off the chosen face,
    // which draws a bright seam along every cube edge. All six faces carry the same params.
    float nDotL = dot(worldNormal, lightDirection);
    float slope = sqrt(max(1.0 - nDotL * nDotL, 0.0));

    float texelWorldSize = lightDistance * shadowViews[viewIndex].params.x;
    float offset = texelWorldSize * shadowViews[viewIndex].params.y * slope;

    vec3 samplePosition = worldPosition + worldNormal * offset;

    // Six views for a point light, one for a spot.
    if (lights[lightIndex].shadowViewCount > 1)
    {
        viewIndex += SelectCubeFace(samplePosition - lights[lightIndex].position);
    }

    return SampleShadowView(viewIndex, samplePosition, 0);
}

// The directional light's shadow term. The cascade is chosen by camera distance. The directional
// light is always lights[0]; see Renderer3D::AddLight.
float ComputeShadowFactor()
{
    int viewIndex = lights[0].shadowViewIndex;

    if (viewIndex < 0)
    {
        return 1.0;
    }

    float fragCameraDistance = length(vIn.worldPosition - vIn.cameraPos);

    // Matches the 10 metre far plane of cascade 0 in ShadowCascades.cpp. Clamped, so a cascade
    // index can never reach into another light's views.
    int cascade = int(fragCameraDistance > 10.0);
    cascade = min(cascade, lights[0].shadowViewCount - 1);

    // No normal offset: front-face culling already provides the separation.
    float shadow = SampleShadowView(viewIndex + cascade, vIn.worldPosition, 1);

    // Never fully black, so the darkest shadow does not read as a hole.
    return max(shadow, 0.1);
}
