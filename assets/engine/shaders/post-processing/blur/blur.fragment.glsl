#version 420 core

#ifndef VERSION_SINGLE_CHANNEL
    #define DATA_TYPE vec4
#else
    #define DATA_TYPE float
#endif

layout(location = 0) out DATA_TYPE m_Output;

in VertexData
{
	vec2 uv;
}vIn;

uniform sampler2D inputBuffer;

uniform int offset;

uniform vec2 texelSize;

DATA_TYPE sampleInputBuffer(vec2 uv)
{
    #ifndef VERSION_SINGLE_CHANNEL
        return texture(inputBuffer, uv);
    #else
        return texture(inputBuffer, uv).r;
    #endif
}

void main(void)
{
    DATA_TYPE output;

    output += sampleInputBuffer(vIn.uv + vec2(-offset, -offset) * texelSize);
    output += sampleInputBuffer(vIn.uv + vec2(-offset, offset) * texelSize);
    output += sampleInputBuffer(vIn.uv + vec2(offset, -offset) * texelSize);
    output += sampleInputBuffer(vIn.uv + vec2(offset, offset) * texelSize);

    m_Output = DATA_TYPE(output * 0.25);
}