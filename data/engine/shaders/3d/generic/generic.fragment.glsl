#version 420 core

#shader bind matSamplers.diffuse 0
#shader bind matSamplers.specular 4
#shader bind matSamplers.normal 8

layout(location = 0) out vec4 m_OutputColor;

in VertexData
{
	vec2 uv;
	vec3 worldPosition;
    vec3 cameraPos;
	vec3 cameraDir;
	float cameraDistance;
	vec3 normalDir;
	vec3 lightDir[8];
    flat int lightIndices[8];
}vIn;

#include "shared/common.glsl"
#include "shared/lightning/lightning.glsl"

uniform MaterialSamplers matSamplers;
uniform bool hasTangentData;

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

    return surface;
}

// Point lights occupy instance light slots 0-4; their directions are vIn.lightDir[1..5].
// Written out with literal subscripts on purpose: a loop that indexes vIn.lightDir[i + 1] reads
// garbage on some drivers (seen on NVIDIA), which zeroes N.L and leaves the surface ambient-only.
vec3 CalculateSpotLights(Surface surface)
{
    vec3 lightColorOutput = vec3(0.f);

    if (vIn.lightIndices[0] != 0) lightColorOutput += CalculateSpotLight(surface, vIn.lightIndices[0], vIn.lightDir[1]);
    if (vIn.lightIndices[1] != 0) lightColorOutput += CalculateSpotLight(surface, vIn.lightIndices[1], vIn.lightDir[2]);
    if (vIn.lightIndices[2] != 0) lightColorOutput += CalculateSpotLight(surface, vIn.lightIndices[2], vIn.lightDir[3]);
    if (vIn.lightIndices[3] != 0) lightColorOutput += CalculateSpotLight(surface, vIn.lightIndices[3], vIn.lightDir[4]);
    if (vIn.lightIndices[4] != 0) lightColorOutput += CalculateSpotLight(surface, vIn.lightIndices[4], vIn.lightDir[5]);

    return lightColorOutput;
}

// The spot light occupies instance light slot 5; its direction is vIn.lightDir[6].
vec3 CalculatePointLights(Surface surface)
{
    vec3 ret = vec3(0.f);

    if (vIn.lightIndices[5] != 0) {
        ret = CalculatePointLight(surface, vIn.lightIndices[5], vIn.lightDir[6]);
    }

    return ret;
}

void main(void)
{
    #shader preFragment

#ifdef VERSION_DISCARD
    vec4 frag = texture(matSamplers.diffuse, vIn.uv * matPropeties[0].uvScale);

    if (frag.w < 0.001f || frag.r + frag.g + frag.b <= 0.5f)
    {
        discard;
    }
#endif

    Surface surface = CreateSurface();

    vec4 directionalLight = vec4(CalculateDirectionalLight(surface), 1.0);
    vec4 spotLights = vec4(CalculateSpotLights(surface), 1.0);
    vec4 pointLights = vec4(CalculatePointLights(surface), 1.0);

    // Sum the rgb terms only: adding the vec4s would leave alpha at 3.0, and the resolve pass passes alpha through.
    m_OutputColor = vec4(directionalLight.rgb + spotLights.rgb + pointLights.rgb, 1.0);

    // Distance fog. fogSettings.x = view distance, fogSettings.y = intensity (0 disables it).
    // Classic linear fog: blends toward fogColor from the camera out to the view distance.
    if (world.fogSettings.y > 0.0)
    {
        float fogFactor = clamp(vIn.cameraDistance / max(world.fogSettings.x, 0.001), 0.0, 1.0) * world.fogSettings.y;
        m_OutputColor.rgb = mix(m_OutputColor.rgb, world.fogColor.rgb, fogFactor);
    }

    #shader postFragment
}
