#version 420 core

layout(location = 0) out vec4 m_OutputColor;

in VertexData
{
	vec3 uv;
}input;

uniform samplerCube skyboxCubeMap;

void main(void)
{
    m_OutputColor = texture(skyboxCubeMap, input.uv);
}