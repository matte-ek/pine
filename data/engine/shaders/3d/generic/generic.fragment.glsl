#version 420 core

#shader bind matSamplers.diffuse 0
#shader bind matSamplers.specular 4
#shader bind matSamplers.normal 8

layout(location = 0) out vec4 m_OutputColor;

#include "shared/common.glsl"
#include "shared/vertex-data.glsl"
#include "shared/lightning/lightning.glsl"

uniform MaterialSamplers matSamplers;
uniform bool hasTangentData;

#shader version VERSION_TRANSPARENT 4

#shader hooks


Surface CreateSurface()
{
    Surface surface;

    MaterialProperties material = matPropeties[0];

    surface.diffuseColor = texture(matSamplers.diffuse, vIn.uv * material.uvScale).xyz * material.diffuseColor + vec3(0.f, 0.f, 0.f);
    surface.specularColor = texture(matSamplers.specular, vIn.uv * material.uvScale).xyz * material.specularColor;
    surface.ambientColor = material.ambientColor;
    surface.shininess = material.shininess;

    if (hasTangentData)
        surface.normal = normalize((2.0 * texture(matSamplers.normal, vIn.uv * material.uvScale) - 1.0).xyz);
    else
        surface.normal = vIn.normalDir;

    // A surface drawn with both of its faces - a leaf card, a sheet of grass - is reached from
    // either side, and the normal it carries was authored for one of them. Seen from the other it
    // points away, so every light in front of the surface reads as behind it and the far side of a
    // canopy shades flat. A thin sheet's far side is the near side's normal negated, which holds
    // for a mapped normal too: the map perturbs around the geometric normal and the perturbation
    // mirrors with it.
    //
    // Unconditional, and no shader version of its own: a face the rasterizer culls never gets here
    // in the first place, so for anything but MaterialRenderFace::Both gl_FrontFacing is always
    // true and this is a no-op.
    if (!gl_FrontFacing)
    {
        surface.normal = -surface.normal;
    }

    return surface;
}

// Point lights occupy instance light slots 0-4; their directions are vIn.lightDir[1..5].
// Written out with literal subscripts on purpose: a loop that indexes vIn.lightDir[i + 1] reads
// garbage on some drivers (seen on NVIDIA), which zeroes N.L so these lights contribute nothing
// and the surface falls back to the flat ambient term alone.
vec3 CalculatePointLights(Surface surface)
{
    vec3 lightColorOutput = vec3(0.f);

    if (vIn.lightIndices[0] != 0) lightColorOutput += CalculatePointLight(surface, vIn.lightIndices[0], vIn.lightDir[1]);
    if (vIn.lightIndices[1] != 0) lightColorOutput += CalculatePointLight(surface, vIn.lightIndices[1], vIn.lightDir[2]);
    if (vIn.lightIndices[2] != 0) lightColorOutput += CalculatePointLight(surface, vIn.lightIndices[2], vIn.lightDir[3]);
    if (vIn.lightIndices[3] != 0) lightColorOutput += CalculatePointLight(surface, vIn.lightIndices[3], vIn.lightDir[4]);
    if (vIn.lightIndices[4] != 0) lightColorOutput += CalculatePointLight(surface, vIn.lightIndices[4], vIn.lightDir[5]);

    return lightColorOutput;
}

// Spot lights occupy instance light slots 5-6; their directions are vIn.lightDir[6..7]. Two slots
// so a hand-held light and a world light can reach the same surface. Same literal-subscript rule as
// the point lights above.
//
// Each slot costs a cone test and a shadow atlas sample, so this is the loop that gets more
// expensive when the count is raised - not the vertex side.
vec3 CalculateSpotLights(Surface surface)
{
    vec3 ret = vec3(0.f);

    if (vIn.lightIndices[5] != 0) ret += CalculateSpotLight(surface, vIn.lightIndices[5], vIn.lightDir[6]);
    if (vIn.lightIndices[6] != 0) ret += CalculateSpotLight(surface, vIn.lightIndices[6], vIn.lightDir[7]);

    return ret;
}

void main(void)
{
    #shader preFragment

    // Both cutout versions decide something from the diffuse texel's alpha before any shading
    // happens, so sample it once up here rather than once per branch.
#if defined(VERSION_DISCARD) || defined(VERSION_TRANSPARENT)
    float diffuseAlpha = texture(matSamplers.diffuse, vIn.uv * matPropeties[0].uvScale).w;
#endif

#ifdef VERSION_DISCARD
    // Coverage test, not an "is there any alpha at all" test. The sampled alpha is a filtered
    // value: along a cutout's edge the texture unit blends the mask's opaque texels with the
    // transparent ones next to it, and blends their colour along with it. Those transparent texels
    // are black in practically every foliage texture, so a texel that is only partly covered comes
    // back with its colour dragged towards black.
    //
    // This version writes alpha 1.0, so whatever survives here is stamped out fully opaque. Keeping
    // every texel with a trace of coverage therefore paints that black ramp as solid geometry - a
    // dark outline one filter-width wide around every blade and leaf. Half coverage is the cut:
    // it is the point where a texel is more inside the mask than outside it.
    if (diffuseAlpha < 0.5f)
    {
        discard;
    }
#endif

#ifdef VERSION_TRANSPARENT
    float surfaceAlpha = matPropeties[0].alpha * diffuseAlpha;

    // A fragment this close to clear cannot change the pixel it lands on: the blend pass runs
    // SourceAlpha / OneMinusSourceAlpha, which weights this fragment's colour by this very alpha
    // and the destination by the rest of it. Shading it anyway costs the whole light loop - every
    // light slot the object holds, each with a shadow atlas tap - and a cutout sheet is mostly
    // hole, so on foliage those are the bulk of the fragments this pass rasterizes. Nothing else
    // is lost by leaving here: the pass writes no depth, so a fragment had nothing to contribute
    // but the colour it was about to weight away.
    //
    // The threshold sits below one 8-bit alpha step (1/255 = 0.0039), so only texels that really
    // are clear are dropped, never one that would have tinted the pixel.
    if (surfaceAlpha < 0.001f)
    {
        discard;
    }
#endif

    Surface surface = CreateSurface();

    // Ambient is per-environment, so it is added once rather than once per light. Direct light from
    // each source is summed on top.
    vec3 ambient = CalculateAmbientLight(surface);
    vec3 directionalLight = CalculateDirectionalLight(surface);
    vec3 pointLights = CalculatePointLights(surface);
    vec3 spotLights = CalculateSpotLights(surface);

    m_OutputColor = vec4(ambient + directionalLight + pointLights + spotLights, 1.0);

#ifdef VERSION_TRANSPARENT
    // The surface's opacity: the material's own alpha, scaled by whatever the diffuse texture
    // carries in its alpha channel. Every other version leaves the 1.0 above alone - the resolve
    // pass forwards this buffer's alpha to the final image, so solid geometry writing less than
    // that would show through the composite.
    m_OutputColor.a = surfaceAlpha;
#endif

    // Distance fog. fogSettings.x = view distance, fogSettings.y = intensity (0 disables it).
    // Classic linear fog: blends toward fogColor from the camera out to the view distance.
    if (world.fogSettings.y > 0.0)
    {
        float fogFactor = clamp(vIn.cameraDistance / max(world.fogSettings.x, 0.001), 0.0, 1.0) * world.fogSettings.y;
        m_OutputColor.rgb = mix(m_OutputColor.rgb, world.fogColor.rgb, fogFactor);
    }

    #shader postFragment
}
