#version 420 core

layout (location = 0) in vec3 m_Vertex;
layout (location = 1) in vec2 m_Uv;

// Instance data
layout (location = 2) in vec4 m_PositionScale;
layout (location = 3) in vec4 m_UvTransform;
layout (location = 4) in vec4 m_Color;
layout (location = 5) in vec2 m_TextureIndexRadius;

out vec3 m_PassVertexPosition;
out vec4 m_PassColor;
out vec4 m_PassUvTransform;
out vec2 m_PassTextureIndexRadius;
out vec2 m_PassUv;

void main()
{
    // Set instance data to the fragment shader
    m_PassVertexPosition = m_Vertex;
    m_PassColor = m_Color;
    m_PassUvTransform = m_UvTransform;
    m_PassTextureIndexRadius = m_TextureIndexRadius;

    // Set vertex UV
    m_PassUv = m_Uv;

    gl_Position = vec4(m_PositionScale.xy, 0, 0) + vec4(m_Vertex.xy, 0, 1) * vec4(m_PositionScale.zw, 0, 1);
}