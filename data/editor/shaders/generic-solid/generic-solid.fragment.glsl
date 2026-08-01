#version 420 core

layout(location = 0) out vec4 m_OutputColor;

layout(std140) uniform Matrices
{
	mat4 projectionMatrix;
	mat4 viewMatrix;
};

uniform vec3 m_Color;

void main(void)
{
	#ifdef VERSION_DISCARD
		discard;
	#endif

    m_OutputColor = vec4(m_Color, 1.f);
}