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

// Point lights occupy instance light slots 0-4; their directions are vIn.lightDir[1..5].
// Literal subscripts on purpose: indexing vIn.lightDir[i + 1] in a loop reads garbage on some
// drivers (seen on NVIDIA).
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

// Spot lights occupy instance light slots 5-6; their directions are vIn.lightDir[6..7]. Same
// literal-subscript rule as the point lights above.
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
    // The material's alpha scaled by the diffuse texture's. Every other version must keep 1.0,
    // because the resolve pass forwards this alpha to the final image.
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
