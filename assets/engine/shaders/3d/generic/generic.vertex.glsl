#version 420 core

layout (location = 0) in vec3 m_Vertex;

layout(std140) uniform Matrix
{
	mat4 m_ProjectionMatrix;
	mat4 m_ViewMatrix;
};

uniform mat4 m_TransformationMatrix;

void main()
{
	vec4 vertexPosition = vec4(m_Vertex, 1.0);

	gl_Position = m_ProjectionMatrix * m_ViewMatrix * m_TransformationMatrix * vertexPosition;
}