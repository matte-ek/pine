#version 420 core

#shader bind sceneBuffer 0
#shader bind ambientOcclusionBuffer 1

layout(location = 0) out vec4 m_OutputColor;

in VertexData
{
	vec2 uv;
}vIn;

uniform sampler2D ambientOcclusionBuffer;
uniform sampler2D sceneBuffer;

uniform vec2 viewportScale;

void main(void)
{
    vec4 frag = texture(sceneBuffer, vec2(vIn.uv.x, 1 - vIn.uv.y) * viewportScale);

    float aoScale = texture(ambientOcclusionBuffer, vec2(vIn.uv.x, vIn.uv.y)).r;

    m_OutputColor = vec4(frag.rgb * aoScale, frag.a);
}