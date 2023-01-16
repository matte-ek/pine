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

    //vec4 color = m_PassColor * texture(m_Textures[textureId], m_PassUv * uvScale + uvOffset);
    vec4 color = vec4(texture(m_Textures[textureId], m_PassUv * uvScale + uvOffset).xyz, 1);

    vec2 centerCornerDistance = max(abs(m_PassVertexPosition.xy) - 1 + radius, 0.0);
    float centerCornerDistanceRatio = (radius / length(centerCornerDistance)) - 1;
    float centerCornerAlphaTransition = clamp(centerCornerDistanceRatio / (0.06f - (radius * 0.1f)), 0, 1);

    color.a *= (centerCornerAlphaTransition + 1 * floor(1 - radius));

    m_OutputColor = color;
}