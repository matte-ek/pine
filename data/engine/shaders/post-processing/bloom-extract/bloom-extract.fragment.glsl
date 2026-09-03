#version 420 core

#shader bind sceneBuffer 0

layout(location = 0) out vec4 m_OutputColor;

in VertexData
{
	vec2 uv;
}vIn;

uniform sampler2D sceneBuffer;

uniform vec2 viewportScale;
uniform float threshold;

// Width of the soft knee above the threshold. A hard cutoff makes bloom pop on/off as pixels cross
// the threshold; the knee fades brightness in over a small range so it ramps smoothly instead.
const float KNEE = 0.5;

void main(void)
{
    // Sample the scene WITHOUT the resolve pass's Y flip. The extract writes into a quad-rendered
    // buffer that the resolve later reads back with plain uv (exactly like the AmbientOcclusion
    // buffer), so the round-trip through this buffer already accounts for orientation. Flipping here
    // as well would double-flip and mirror the glow vertically relative to the scene. viewportScale
    // still maps into the used sub-region. The scene buffer is HDR, so color can carry values above 1.
    vec3 color = texture(sceneBuffer, vIn.uv * viewportScale).rgb;

    // Brightness of the pixel (its brightest channel), and how far it pokes above the threshold.
    float brightness = max(color.r, max(color.g, color.b));
    float contribution = smoothstep(threshold, threshold + KNEE, brightness);

    // Keep the HDR color, scaled by how strongly it qualifies as "bright". Everything below the
    // threshold contributes 0, so only highlights bloom.
    m_OutputColor = vec4(color * contribution, 1.0);
}
