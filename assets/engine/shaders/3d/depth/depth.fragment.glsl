#version 420 core

layout(location = 0) out vec4 m_Output;

layout(std140) uniform Matrices
{
	mat4 projectionMatrix;
	mat4 viewMatrix;
};

in VertexData
{
    vec3 normal;
}vIn;

#shader hooks

void main(void)
{
    #shader preFragment

    // TODO: Maybe encode additional data in the alpha channel?
	m_Output = vec4(normalize(vIn.normal), 1.0);

    #shader postFragment
}