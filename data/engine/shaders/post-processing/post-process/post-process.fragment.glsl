#version 420 core

#shader bind sceneBuffer 0
#shader bind ambientOcclusionBuffer 1
#shader bind bloomBuffer 2

layout(location = 0) out vec4 m_OutputColor;

in VertexData
{
	vec2 uv;
}vIn;

uniform sampler2D ambientOcclusionBuffer;
uniform sampler2D sceneBuffer;
uniform sampler2D bloomBuffer;

uniform vec2 viewportScale;
uniform float time;
uniform float grainStrength;
uniform float vignetteStrength;
uniform float exposure;
uniform float bloomIntensity;

// Sine-free hash (Dave Hoskins style). The sin(dot(...)) hash has straight iso-lines that show
// up as diagonal banding in grain; this 3D variant stays random and takes time as a third axis
// so every frame is a fresh noise field instead of a shifted copy of the same one.
float hash(vec3 p)
{
    p = fract(p * 0.1031);
    p += dot(p, p.zyx + 31.32);
    return fract((p.x + p.y) * p.z);
}

// ACES filmic tone mapping (Krzysztof Narkowicz's fit). Maps open-ended HDR values into [0,1],
// rolling highlights off toward white instead of hard-clipping them. This is what lets a bright
// light "blow out" gracefully rather than clamping to flat white.
vec3 ACESFilm(vec3 x)
{
    const float a = 2.51;
    const float b = 0.03;
    const float c = 2.43;
    const float d = 0.59;
    const float e = 0.14;
    return clamp((x * (a * x + b)) / (x * (c * x + d) + e), 0.0, 1.0);
}

void main(void)
{
    vec4 frag = texture(sceneBuffer, vec2(vIn.uv.x, 1 - vIn.uv.y) * viewportScale);

    float aoScale = texture(ambientOcclusionBuffer, vec2(vIn.uv.x, vIn.uv.y)).r;

    // --- HDR / linear space ---
    // The scene buffer is RGBA16F, so frag.rgb can hold values well above 1.0. Ambient occlusion is a
    // scaling of scene light; bloom is the blurred glow of bright areas, added on top (not occluded).
    vec3 color = frag.rgb * aoScale;
    color += texture(bloomBuffer, vIn.uv).rgb * bloomIntensity;

    // Exposure applies to the whole HDR image, then tone map -> [0,1] and encode linear -> sRGB.
    color *= exposure;
    color = ACESFilm(color);
    color = pow(color, vec3(1.0 / 2.2));

    // --- display space ([0,1]) ---
    // Vignette and film grain are display-space looks; their strengths are tuned against [0,1] values.

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
