#version 420 core

#shader bind sceneNormalBuffer 0
#shader bind sceneDepthBuffer 1
#shader bind kernelRandomnessTexture 2

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

// The pre-pass buffers are allocated at the internal resolution but filled only in the corner this
// context rendered into, so every lookup into them is scaled into that corner. The view coordinate
// itself is not: position reconstruction works in the full [0,1] of the view, not of the texture.
uniform vec2 viewportScale;

const vec2 noiseScale = vec2(960.0 / 4.0, 540.0 / 4.0);

const float radius = 0.8f;

// Number of hemisphere samples taken per fragment. The kernel buffer holds 64
// entries; we only consume the first sampleCount of them. Lower = faster.
// Driven at runtime by the graphics settings (clamped to [1, 64] on the CPU).
uniform int sampleCount;

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

// View-space Z from a depth value without a full matrix multiply.
//
// For clip = (0, 0, z_ndc, 1), invProjectionMatrix * clip only touches column 2
// (scaled by z_ndc) and column 3, so view.z and view.w each reduce to a
// linear expression in z_ndc. Pass the four relevant matrix components in
// 'coeff' (precomputed once per fragment) and this collapses to a couple of
// mul/adds instead of a mat4 * vec4 for every sample in the loop.
//
//   coeff = (invProj[2][2], invProj[3][2], invProj[2][3], invProj[3][3])
float ReconstructViewZ(float depth, vec4 coeff)
{
    float z_ndc = depth * 2.0 - 1.0; // [0,1] → [-1,1]
    return (coeff.x * z_ndc + coeff.y) / (coeff.z * z_ndc + coeff.w);
}

void main(void)
{

    float depth = texture(sceneDepthBuffer, vIn.uv * viewportScale).r;

    if (depth >= 1.f)
    {
        m_Output = 1.0;
        return;
    }

    vec3 position = ReconstructPosition(vIn.uv, depth);
    vec3 normal = texture(sceneNormalBuffer, vIn.uv * viewportScale).xyz;
    vec3 kernelRandomNoise = texture(kernelRandomnessTexture, vIn.uv * noiseScale).xyz;

    vec3 tangent = normalize(kernelRandomNoise - normal * dot(kernelRandomNoise, normal));
    vec3 bitangent = cross(normal, tangent);
    mat3 tangentSpaceMatrix = mat3(tangent, bitangent, normal);

    // Precompute the depth → view-space-Z coefficients once, so the per-sample
    // reconstruction avoids a full matrix multiply (see ReconstructViewZ).
    vec4 viewZCoeff = vec4(
        invProjectionMatrix[2][2],
        invProjectionMatrix[3][2],
        invProjectionMatrix[2][3],
        invProjectionMatrix[3][3]);

    float occlusion = 0.0;

    for (int i = 0; i < sampleCount;i++)
    {
        vec3 samplePos = position + (tangentSpaceMatrix * kernel[i].xyz) * radius;

        vec4 offset = projectionMatrix * vec4(samplePos, 1);
        offset.xyz /= offset.w;

        // Convert from [-1, 1] to [0, 1] to sample
        vec2 screenOffset = offset.xy * 0.5 + 0.5;

        depth = texture(sceneDepthBuffer, screenOffset * viewportScale).r;

        float sampleDepth = ReconstructViewZ(depth, viewZCoeff);

        float rangeCheck = smoothstep(0.0, 1.0, radius / abs(sampleDepth - samplePos.z));

        occlusion += (sampleDepth >= samplePos.z ? 1.0 : 0.0) * rangeCheck;
    }

    // Normalise to match the previous look: the old code accumulated up to 64
    // samples and divided by 128 (i.e. 2 * sample count).
    occlusion /= float(sampleCount) * 2.0;

    m_Output = 1.0 - pow(occlusion, 0.5); 
}
