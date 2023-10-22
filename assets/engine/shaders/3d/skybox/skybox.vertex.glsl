#version 420 core

layout (location = 0) in vec3 vertex;

layout(std140) uniform Matrices
{
	mat4 projectionMatrix;
	mat4 viewMatrix;
};

out VertexData
{
	vec3 uv;
}output;

void main()
{
	vec4 wPos = projectionMatrix * mat4(mat3(viewMatrix)) * vec4(vertex, 1.0);

    output.uv = vertex;

    gl_Position = wPos.xyww;
}  