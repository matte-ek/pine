#version 420 core

layout(location = 0) out vec4 m_OutputColor;

in VertexData
{
	vec3 uv;
}vIn;

uniform samplerCube skyboxCubeMap;

void main(void)
{
    m_OutputColor = texture(skyboxCubeMap, vIn.uv);
}