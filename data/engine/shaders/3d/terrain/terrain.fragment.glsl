#version 420 core

// Four units per texture type are reserved by Specifications::Samplers (diffuse 0-3, specular 4-7,
// normal 8-11), which is what sets the layer count. 12 is the first free unit after them; 17 is the
// shadow atlas, bound by shared/lightning/shadows.glsl.
#shader bind layerDiffuse 0-4
#shader bind layerSpecular 4-4
#shader bind layerNormal 8-4
#shader bind splatMap 12

// The editor's brush overlay, compiled only when the editor asks for this version - a built game
// never requests it, so it neither compiles nor pays for the code below. Declared here rather than
// in terrain.ih because only EngineCli's batch import reads that file, and running it would re-mint
// this shader's UId.
#shader version VERSION_BRUSH 1

layout(location = 0) out vec4 m_OutputColor;

#include "shared/common.glsl"
#include "shared/vertex-data.glsl"
#include "shared/lightning/lightning.glsl"
#include "shared/fog.glsl"

uniform sampler2D layerDiffuse[TERRAIN_LAYER_COUNT];
uniform sampler2D layerSpecular[TERRAIN_LAYER_COUNT];
uniform sampler2D layerNormal[TERRAIN_LAYER_COUNT];

// Per-texel layer weights over the whole terrain, one texel per height sample. One texture rather
// than one per chunk so that the blend filters across a chunk edge the same way it filters anywhere
// else - a per-chunk texture would need duplicated border texels to avoid a visible seam there.
uniform sampler2D splatMap;

// Maps vIn.uv - terrain-local world units - onto that texture: xy scales, zw offsets. A uniform
// rather than a second uv stream on the mesh, which Mesh does not carry and which would have to be
// rebuilt for every detail level of every chunk.
uniform vec4 splatTransform;

#ifdef VERSION_BRUSH

// Where the editor's sculpting brush is: xy the centre in terrain-local units, z the radius, w the
// width of the ring drawn at its rim. Only the editor ever asks for this version of the shader, so
// a built game neither compiles nor pays for any of it.
uniform vec4 brushRing;

// Tints the ground under the brush, so that the author can see what a stroke is about to reach.
//
// Drawn on the surface rather than as a line ring floating over it: a ring has to follow ground
// that is not flat, and the one thing that is guaranteed to follow it exactly is the ground's own
// fragments. The distance is measured in the xz plane, which is also how the brush itself decides
// which samples it covers - what is highlighted is what will move.
vec3 ApplyBrushRing(vec3 color)
{
	float distance = length(vIn.uv - brushRing.xy);

	if (distance > brushRing.z)
	{
		return color;
	}

	// A wash over the whole disc, and a brighter band at the rim so the edge of the brush reads
	// even against ground that is already bright.
	float rim = 1.0 - smoothstep(0.0, max(brushRing.w, 0.001), brushRing.z - distance);

	return mix(color * 1.25 + vec3(0.04), vec3(1.0, 0.85, 0.25), rim * 0.65);
}

#endif

// One layer's contribution, at the weight the splat map gives it here.
//
// Called with literal layer indices only. Indexing a sampler array with a variable is not a
// dynamically uniform expression here and is undefined, which is the same reason vIn.lightDir[] is
// only ever subscripted with literals.
void AddLayer(inout Surface surface,
              inout vec3 tangentNormal,
              sampler2D diffuse,
              sampler2D specular,
              sampler2D normalMap,
              MaterialProperties material,
              float weight)
{
    vec2 layerUv = vIn.uv * material.uvScale;

    surface.diffuseColor += weight * texture(diffuse, layerUv).xyz * material.diffuseColor;
    surface.specularColor += weight * texture(specular, layerUv).xyz * material.specularColor;
    surface.ambientColor += weight * material.ambientColor;
    surface.shininess += weight * material.shininess;

    // Summed in tangent space and normalized once at the end, rather than normalized per layer:
    // the weighted sum of unit vectors is what makes two layers meeting produce the average of
    // their bumps instead of whichever one happens to win.
    tangentNormal += weight * DecodeNormalMap(texture(normalMap, layerUv));
}

Surface CreateSurface()
{
    vec4 weights = texture(splatMap, vIn.uv * splatTransform.xy + splatTransform.zw);

    // The stored weights already sum to one, but this is cheap insurance against the two ways that
    // stops holding: a terrain saved before its weights existed reads as all zeroes, and the
    // renormalization keeps the surface at the brightness its layers describe rather than scaling
    // the whole ground with whatever the weights happen to total.
    float total = weights.r + weights.g + weights.b + weights.a;

    weights = total > 0.0001 ? weights / total : vec4(1.0, 0.0, 0.0, 0.0);

    Surface surface;

    surface.diffuseColor = vec3(0.0);
    surface.specularColor = vec3(0.0);
    surface.ambientColor = vec3(0.0);
    surface.shininess = 0.0;

    vec3 tangentNormal = vec3(0.0);

    AddLayer(surface, tangentNormal, layerDiffuse[0], layerSpecular[0], layerNormal[0], matPropeties[0], weights.r);
    AddLayer(surface, tangentNormal, layerDiffuse[1], layerSpecular[1], layerNormal[1], matPropeties[1], weights.g);
    AddLayer(surface, tangentNormal, layerDiffuse[2], layerSpecular[2], layerNormal[2], matPropeties[2], weights.b);
    AddLayer(surface, tangentNormal, layerDiffuse[3], layerSpecular[3], layerNormal[3], matPropeties[3], weights.a);

    surface.normal = normalize(tangentNormal);

    return surface;
}

void main(void)
{
    Surface surface = CreateSurface();

    vec3 ambient = CalculateAmbientLight(surface);
    vec3 directionalLight = CalculateDirectionalLight(surface);
    vec3 pointLights = CalculatePointLights(surface);
    vec3 spotLights = CalculateSpotLights(surface);

    vec3 lighting = ambient + directionalLight + pointLights + spotLights;

#ifdef VERSION_BRUSH
    lighting = ApplyBrushRing(lighting);
#endif

    m_OutputColor = vec4(lighting, 1.0);

    m_OutputColor.rgb = ApplySurfaceFog(m_OutputColor.rgb, vIn.cameraPos, vIn.worldPosition);
}
