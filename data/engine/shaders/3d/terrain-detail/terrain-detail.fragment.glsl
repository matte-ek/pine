#version 420 core

#shader bind matSamplers.diffuse 0
#shader bind matSamplers.specular 4
#shader bind matSamplers.normal 8

// Cutout, for foliage. Renderer3D::PrepareTerrainDetailMesh asks for it for every material that is
// not Opaque, Transparent ones included: detail has no sorted blend pass to draw those in.
#shader version VERSION_DISCARD 1

layout(location = 0) out vec4 m_OutputColor;

// The normal leans towards the ground's (see the vertex stage), so it belongs to neither face and
// both faces of a two-sided card shade with it unflipped.
#define PINE_FACE_INDEPENDENT_NORMAL

#include "shared/common.glsl"
#include "shared/vertex-data.glsl"
#include "shared/lightning/lightning.glsl"
#include "shared/fog.glsl"

uniform MaterialSamplers matSamplers;
uniform bool hasTangentData;

Surface CreateSurface()
{
    Surface surface;

    MaterialProperties material = matPropeties[0];

    surface.diffuseColor = texture(matSamplers.diffuse, vIn.uv * material.uvScale).xyz * material.diffuseColor;
    surface.specularColor = texture(matSamplers.specular, vIn.uv * material.uvScale).xyz * material.specularColor;
    surface.ambientColor = material.ambientColor;
    surface.shininess = material.shininess;

    if (hasTangentData)
    {
        surface.normal = DecodeNormalMap(texture(matSamplers.normal, vIn.uv * material.uvScale));
    }
    else
    {
        surface.normal = vIn.normalDir;
    }

    return surface;
}

void main(void)
{
#ifdef VERSION_DISCARD
    // Half coverage, for the same reason as the generic shader's Discard version: filtered edge
    // texels are blended towards black, and keeping them would outline every blade.
    if (texture(matSamplers.diffuse, vIn.uv * matPropeties[0].uvScale).w < 0.5f)
    {
        discard;
    }
#endif

    Surface surface = CreateSurface();

    vec3 ambient = CalculateAmbientLight(surface);
    vec3 directionalLight = CalculateDirectionalLight(surface);
    vec3 pointLights = CalculatePointLights(surface);
    vec3 spotLights = CalculateSpotLights(surface);

    m_OutputColor = vec4(ambient + directionalLight + pointLights + spotLights, 1.0);
    m_OutputColor.rgb = ApplySurfaceFog(m_OutputColor.rgb, vIn.cameraPos, vIn.worldPosition);
}
