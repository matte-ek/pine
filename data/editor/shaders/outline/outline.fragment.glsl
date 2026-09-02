#version 420 core

// Screen-space outline pass.
//
// Reads a silhouette mask (selected objects rendered flat white) and paints a
// constant-width halo just outside the silhouette. Because the thickness is
// measured in mask texels (= screen pixels), the outline stays exactly
// 'outlineWidth' pixels thick on every model, at any distance or scale --
// unlike scaling the mesh in object space.

#shader bind maskBuffer 0

layout(location = 0) out vec4 m_OutputColor;

uniform sampler2D maskBuffer;

uniform vec3 outlineColor;
uniform int outlineWidth;   // outline thickness, in pixels
uniform vec2 bufferSize;    // mask/scene buffer size, in pixels

// Upper bound for the sampling kernel; the actual radius used is outlineWidth.
const int MAX_RADIUS = 8;

void main(void)
{
    vec2 texel = 1.0 / bufferSize;

    // gl_FragCoord shares the mask's pixel space (same framebuffer size and
    // viewport), so we can sample the mask directly without any UV flip.
    vec2 uv = gl_FragCoord.xy * texel;

    // Don't draw over the object itself, only around it.
    if (texture(maskBuffer, uv).r > 0.5)
    {
        discard;
    }

    // Dilation: this pixel is part of the outline if any mask pixel within
    // 'outlineWidth' is set. The disc test keeps corners rounded and uniform.
    float coverage = 0.0;

    for (int x = -MAX_RADIUS; x <= MAX_RADIUS; x++)
    {
        for (int y = -MAX_RADIUS; y <= MAX_RADIUS; y++)
        {
            if (x * x + y * y > outlineWidth * outlineWidth)
            {
                continue;
            }

            coverage = max(coverage, texture(maskBuffer, uv + vec2(x, y) * texel).r);
        }
    }

    if (coverage < 0.01)
    {
        discard;
    }

    m_OutputColor = vec4(outlineColor, coverage);
}
