// Every shadow in the engine is sampled from here: one atlas, one sampler, one lookup.
//
// sampler2DShadow, so a single texture() call is a hardware 2x2 PCF tap. That matters because a
// fragment can carry up to seven shadowed light slots at once, and filter width is the wall.
uniform sampler2DShadow ShadowAtlas;

#shader bind ShadowAtlas 17

// Samples one shadow view's tile and returns its shadow term, or 1.0 (fully lit) for anything the
// view cannot answer for.
//
// Takes the sample position as given and applies no bias of its own. Bias policy belongs to whoever
// knows what kind of shadow source this is - a cascade separates with front-face culling, a local
// light with a normal offset - and there is no offset that is correct for both. A constant in
// *projected* depth especially is not: it is linear in world depth for an ortho cascade and grows
// with the square of the distance from the light for a perspective one, which is how this used to
// slide a spot light's shadow the better part of a metre away from its caster.
//
// 'pcfTaps' is the radius of a grid of hardware taps: 0 is a single 2x2 tap, 1 is a 3x3 grid of
// them. Local lights take the cheap one because a fragment may sample several; a cascade takes the
// wide one because it is at most one per fragment and its texels cover far more world.
//
// Indexing shadowViews[] dynamically is fine: it is a uniform-block array. The hand-unrolled
// subscripts elsewhere in these shaders exist because vIn.lightDir[] is a *varying* array, which is
// the thing that misbehaves on NVIDIA.
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

    // Outside this view, or past its far plane: no depth to compare against. Returning "lit" is what
    // avoids a dark band at a cascade edge or past the last one.
    if (projected.z > 1.0 ||
        any(lessThan(projected.xy, vec2(0.0))) || any(greaterThan(projected.xy, vec2(1.0))))
    {
        return 1.0;
    }

    // One atlas texel, measured in this view's own units - projected.xy spans one tile, so an atlas
    // texel is texelSize / tileRect.zw of it. Getting this wrong by the tile-to-atlas ratio is what
    // used to let a tap reach into the neighbouring tile, which belongs to a different light.
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

    // params.z fades the shadow in and out as a light gains or loses its tile, so the transition is
    // not a hard pop. It is 1.0 whenever the light holds a stable tile, and always 1.0 for a cascade,
    // which never competes for its own.
    return mix(1.0, shadow, view.params.z);
}

// Which of a point light's six views covers this direction.
//
// Major axis of L, in the same order Shadows.cpp builds the faces: +X, -X, +Y, -Y, +Z, -Z. The
// boundaries land exactly on the 45 degree diagonals, which is where the faces meet; each face is
// rendered slightly wider than 90 degrees so the tap at a boundary still reads texels that face
// actually drew.
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

    // Normal-offset: push the sample point off the surface along its normal before projecting.
    // This is what replaces the cascades' front-face culling, which peter-pans badly at the short
    // ranges a local light works over and breaks outright on single-sided geometry.
    //
    // Measured in shadow-map texels, not in world units, because that is what acne is made of: the
    // tile stores one depth for a texel, and the receiver's own depth drifts away from it across the
    // rest of that texel's world footprint. Sizing the offset to the footprint means it tracks a
    // tile demotion, a wide cone and a distant fragment for free - params.x is the world size of one
    // of this view's texels per unit distance from the light, params.y the offset in texels.
    //
    // sin(angle between the normal and the light) is the scale: zero where the surface faces the
    // light head on and a texel's footprint is flat, largest where the drift across one is worst. It
    // is the bounded stand-in for the tan() an exact depth correction would want, and the grazing
    // angles where the two disagree are the ones N.L has already faded to nothing.
    //
    // Done *before* the face pick, not after. Picking the face from the un-offset position and then
    // offsetting can carry the sample past the edge of the face that was chosen - it then projects
    // outside [0,1] and is treated as unshadowed, which draws a bright seam along every cube edge.
    // The face border is about one texel of angle and the offset is a small multiple of a texel of
    // world, so the offset wins comfortably. All six faces carry identical params, so reading them
    // off the first one is exact rather than approximate.
    float nDotL = dot(worldNormal, lightDirection);
    float slope = sqrt(max(1.0 - nDotL * nDotL, 0.0));

    float texelWorldSize = lightDistance * shadowViews[viewIndex].params.x;
    float offset = texelWorldSize * shadowViews[viewIndex].params.y * slope;

    vec3 samplePosition = worldPosition + worldNormal * offset;

    // A point light spends six views, a spot one. Branching on the *count* rather than on a light
    // type keeps this as ignorant of light types as the CPU side is.
    if (lights[lightIndex].shadowViewCount > 1)
    {
        viewIndex += SelectCubeFace(samplePosition - lights[lightIndex].position);
    }

    return SampleShadowView(viewIndex, samplePosition, 0);
}

// The directional light's shadow term.
//
// Same atlas, same lookup as every other light since the cascades were folded in - the only thing
// still specific to it is how the sub-view is chosen, which is by camera distance rather than by
// cube face. It is lights[0] by construction: Renderer3D::AddLight gives the directional light that
// slot and every other light one above it.
float ComputeShadowFactor()
{
    int viewIndex = lights[0].shadowViewIndex;

    if (viewIndex < 0)
    {
        return 1.0;
    }

    float fragCameraDistance = length(vIn.worldPosition - vIn.cameraPos);

    // Matches the 10 metre far plane of cascade 0 in Shadows.cpp. Clamped rather than assumed, so
    // raising CASCADE_COUNT without adding a threshold here degrades to the last cascade instead of
    // reading a view belonging to some spot light.
    int cascade = int(fragCameraDistance > 10.0);
    cascade = min(cascade, lights[0].shadowViewCount - 1);

    // No normal offset: a cascade renders with front-face culling, which already provides the
    // separation the offset exists to buy, and stacking the two peter-pans.
    float shadow = SampleShadowView(viewIndex + cascade, vIn.worldPosition, 1);

    // Never fully black. Shadowed geometry still picks up ambient and this keeps the darkest result
    // from reading as a hole in the world.
    return max(shadow, 0.1);
}
