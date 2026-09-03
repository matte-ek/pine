#version 420 core

layout(location = 0) out vec4 m_OutputColor;

in VertexData
{
	vec3 uv;
}vIn;

uniform samplerCube skyboxCubeMap;

void main(void)
{
    // The cubemap uses an sRGB internal format, so this sample is already linear (hardware-decoded);
    // it lands in the HDR scene buffer and is tone-mapped + re-encoded in the post-process resolve.
    m_OutputColor = texture(skyboxCubeMap, vIn.uv);
}