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

vec3 CalculateSpotLights(Surface surface)
{
    vec3 lightColorOutput = vec3(0.f);

    for (int i = 0; i < 5;i++) {
        if (vIn.lightIndices[i] != 0) {
            lightColorOutput += CalculateSpotLight(surface, vIn.lightIndices[i], i + 1);
        }
    }

    return lightColorOutput;
}

vec3 CalculatePointLights(Surface surface)
{
    vec3 ret = vec3(0.f);

    if (vIn.lightIndices[5] != 0) {
        ret = CalculatePointLight(surface, vIn.lightIndices[5], 6);
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

    m_OutputColor = directionalLight;

    #shader postFragment
}