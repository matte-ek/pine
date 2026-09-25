#version 420 core

layout(location = 0) out vec4 m_OutputColor;

#include "shared/common.glsl"
#include "shared/fog.glsl"

in VertexData
{
	vec3 uv;
	flat vec3 cameraPosition;
}vIn;

uniform samplerCube skyboxCubeMap;

void main(void)
{
    // The cubemap uses an sRGB internal format, so this sample is already linear (hardware-decoded);
    // it lands in the HDR scene buffer and is tone-mapped + re-encoded in the post-process resolve.
    vec4 skyColor = texture(skyboxCubeMap, vIn.uv);

    // The cube is drawn around the camera unrotated, so its vertex is also the world-space direction
    // this pixel looks along.
    m_OutputColor = vec4(ApplySkyFog(skyColor.rgb, vIn.cameraPosition, vIn.uv), skyColor.a);
}
