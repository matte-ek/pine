#version 420 core

layout(location = 0) out vec4 m_OutputColor;

in VertexData
{
	vec2 uv;
}vIn;

uniform sampler2D sceneBuffer;
uniform vec2 viewportScale;

void main(void)
{
    vec4 frag = texture(sceneBuffer, vec2(vIn.uv.x, 1 - vIn.uv.y) * viewportScale);

    float gammaFactor = 2.2;
    
    m_OutputColor = vec4(frag.rgb, 1);
    //m_OutputColor = vec4(pow(frag.rgb, vec3(1.0 / gammaFactor)), 1);
}