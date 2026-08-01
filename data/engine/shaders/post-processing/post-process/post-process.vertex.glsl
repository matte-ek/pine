#version 420 core

layout(location = 0) in vec3 vertex;
layout(location = 1) in vec2 uv;

out VertexData
{
	vec2 uv;
}vOut;

void main()
{
    vOut.uv = uv;

    gl_Position = vec4(vertex, 1.0);
}