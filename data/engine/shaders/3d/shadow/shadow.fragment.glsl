#version 420 core

// Cutout, for Discard materials, so a leaf card casts the shadow of its leaf rather than of its
// quad. Renderer3D asks for it under MaterialSetupMode::Cutout; it is the same bit as the generic
// shader's Discard version, which is the one that mode requests.
#shader version VERSION_DISCARD 1

#include "shared/common.glsl"

#ifdef VERSION_DISCARD
// Bound in the GLSL rather than with '#shader bind', which is applied to every version: the default
// one never samples this, so the uniform would be optimized out and each compile would warn.
// Binding 0 is Renderer3D::Specifications::Samplers::BASE_DIFFUSE.
layout(binding = 0) uniform sampler2D diffuseMap;

in VertexData
{
    vec2 uv;
}vIn;
#endif

#shader hooks

void main(void)
{
    #shader preFragment

#ifdef VERSION_DISCARD
    if (texture(diffuseMap, vIn.uv * matPropeties[0].uvScale).w < ALPHA_CUTOFF)
    {
        discard;
    }
#endif

    #shader postFragment
}
