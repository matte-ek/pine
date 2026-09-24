#version 420 core

#shader bind matSamplers.diffuse 0
#shader bind matSamplers.specular 4
#shader bind matSamplers.normal 8

layout(location = 0) out vec4 m_OutputColor;

#include "shared/common.glsl"
#include "shared/vertex-data.glsl"
#include "shared/lightning/lightning.glsl"
#include "shared/fog.glsl"

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
        surface.normal = DecodeNormalMap(texture(matSamplers.normal, vIn.uv * material.uvScale));
    else
        surface.normal = vIn.normalDir;

    // A two-sided surface (a leaf card, a sheet of grass) seen from behind needs its normal
    // flipped, or it shades as if lit from behind. A no-op for anything not drawn with
    // MaterialRenderFace::Both, whose back faces are culled.
    if (!gl_FrontFacing)
    {
        surface.normal = -surface.normal;
    }

    return surface;
}

void main(void)
{
    #shader preFragment

    // Sampled once for both cutout versions.
#if defined(VERSION_DISCARD) || defined(VERSION_TRANSPARENT)
    float diffuseAlpha = texture(matSamplers.diffuse, vIn.uv * matPropeties[0].uvScale).w;
#endif

#ifdef VERSION_DISCARD
    // Half coverage, not "any alpha at all": filtered edge texels are blended towards the black of
    // the transparent texels next to them, and keeping those would draw a dark outline around every
    // leaf, since this version writes them fully opaque.
    if (diffuseAlpha < 0.5f)
    {
        discard;
    }
#endif

#ifdef VERSION_TRANSPARENT
    float surfaceAlpha = matPropeties[0].alpha * diffuseAlpha;

    // A fragment this close to clear cannot change the pixel, so skip the light loop for it - on
    // foliage, most fragments are hole. The pass writes no depth, so nothing else is lost. The
    // threshold is below one 8-bit alpha step.
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
    // The material's alpha scaled by the diffuse texture's.
    m_OutputColor.a = surfaceAlpha;
#endif

    m_OutputColor.rgb = ApplyDistanceFog(m_OutputColor.rgb);

    #shader postFragment
}
