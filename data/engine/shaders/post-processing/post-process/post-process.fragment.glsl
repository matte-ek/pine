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
uniform float time;
uniform float grainStrength;
uniform float vignetteStrength;

// Sine-free hash (Dave Hoskins style). The sin(dot(...)) hash has straight iso-lines that show
// up as diagonal banding in grain; this 3D variant stays random and takes time as a third axis
// so every frame is a fresh noise field instead of a shifted copy of the same one.
float hash(vec3 p)
{
    p = fract(p * 0.1031);
    p += dot(p, p.zyx + 31.32);
    return fract((p.x + p.y) * p.z);
}

void main(void)
{
    vec4 frag = texture(sceneBuffer, vec2(vIn.uv.x, 1 - vIn.uv.y) * viewportScale);

    float aoScale = texture(ambientOcclusionBuffer, vec2(vIn.uv.x, vIn.uv.y)).r;

    vec3 color = frag.rgb * aoScale;

    // Vignette: darken toward the corners for a more claustrophobic frame.
    // inner (0.3) = where darkening starts, outer (0.75) = fully dark, 0.5 = corner brightness.
    float vignetteDist = distance(vIn.uv, vec2(0.5));
    float vignette = 1.0 - smoothstep(0.3, 0.75, vignetteDist);
    color *= mix(1.0 - vignetteStrength, 1.0, vignette);

    // Animated film grain. gl_FragCoord gives per-pixel variation; floor(time * 24) advances the
    // noise field at a filmic ~24 fps so it flickers rather than smears.
    float grain = hash(vec3(gl_FragCoord.xy, floor(time * 24.0))) - 0.5;
    color += grain * grainStrength;

    m_OutputColor = vec4(color, frag.a);
}