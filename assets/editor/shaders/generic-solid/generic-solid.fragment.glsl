#version 420 core

layout(location = 0) out vec4 m_OutputColor;

layout(std140) uniform Matrices
{
	mat4 projectionMatrix;
	mat4 viewMatrix;
};

in VertexData
{
	vec2 uv;
	vec3 worldPosition;
}vIn;

uniform vec3 m_Color;

void main(void)
{
    m_OutputColor = vec4(m_Color, 1.f);
}