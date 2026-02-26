#version 420 core

layout(location = 0) out float m_Output;

in VertexData
{
	vec2 uv;
}vIn;

uniform sampler2D sceneNormalBuffer;
uniform sampler2D sceneDepthBuffer;
uniform sampler2D kernelRandomnessTexture;

layout(std140) uniform KernelData
{
	vec4 kernel[64];
};

uniform mat4 projectionMatrix;
uniform mat4 invProjectionMatrix;

const vec2 noiseScale = vec2(960.0 / 4.0, 540.0 / 4.0);

const float radius = 0.8f;

vec3 ReconstructPosition(vec2 uvPos, float depth)
{
    vec4 ndc;

    // Rebuild the NDC by converting to [-1, 1]
    ndc.xy = uvPos * 2.0 - 1.0;
    ndc.z = depth * 2.0 - 1.0;
    ndc.w = 1.0;

    // Inverse the NDC using the inverted camera projection matrix 
    vec4 viewPos = invProjectionMatrix * ndc;
    viewPos /= viewPos.w;

    return viewPos.xyz;
}

float ReconstructViewZ(float depth)
{
    float z_ndc = depth * 2.0 - 1.0; // [0,1] → [-1,1]
    vec4 clip = vec4(0.0, 0.0, z_ndc, 1.0);
    vec4 view = invProjectionMatrix * clip;
    return view.z / view.w;
}

void main(void)
{
    float depth = texture(sceneDepthBuffer, vIn.uv).r;

    if (depth >= 1.f)
    {
        m_Output = 1.0;
        return;
    }

    vec3 position = ReconstructPosition(vIn.uv, depth);
    vec3 normal = texture(sceneNormalBuffer, vIn.uv).xyz;
    vec3 kernelRandomNoise = texture(kernelRandomnessTexture, vIn.uv * noiseScale).xyz;

    vec3 tangent = normalize(kernelRandomNoise - normal * dot(kernelRandomNoise, normal));
    vec3 bitangent = cross(normal, tangent);
    mat3 tangentSpaceMatrix = mat3(tangent, bitangent, normal);

    float occlusion = 0.0;

    for (int i = 0; i < 64;i++)
    {
        vec3 samplePos = position + (tangentSpaceMatrix * kernel[i].xyz) * radius;

        vec4 offset = projectionMatrix * vec4(samplePos, 1);
        offset.xyz /= offset.w;

        // Convert from [-1, 1] to [0, 1] to sample
        vec2 screenOffset = offset.xy * 0.5 + 0.5;

        depth = texture(sceneDepthBuffer, screenOffset).r;

        float sampleDepth = ReconstructViewZ(depth);

        float rangeCheck = smoothstep(0.0, 1.0, radius / abs(sampleDepth - samplePos.z));

        occlusion += (sampleDepth >= samplePos.z ? 1.0 : 0.0) * rangeCheck;
    }

    occlusion /= 64.0;

    m_Output = 1.0 - pow(occlusion, 0.5); 
}