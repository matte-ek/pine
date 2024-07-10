#version 420 core

out vec4 m_OutputColor;

in vec3 m_PassVertexPosition;
in vec4 m_PassColor;
in vec4 m_PassUvTransform;
in vec2 m_PassTextureIndexRadius;
in vec2 m_PassUv;

uniform sampler2D m_Textures[16];

void main()
{
    const vec2 uvOffset = m_PassUvTransform.xy;
    const vec2 uvScale = m_PassUvTransform.zw;
    const float radius = m_PassTextureIndexRadius.y;
    const int textureId = int(m_PassTextureIndexRadius.x);

    m_OutputColor = texture(m_Textures[textureId], m_PassUv * uvScale + uvOffset).a == 0.f ? vec4(0.f) : m_PassColor;
}