#pragma once
#include <cstdint>

namespace Pine::Rendering::GraphicsSettings
{
    enum class QualityPreset : std::int32_t
    {
        Low = 0,
        Medium = 1,
        High = 2,
        Ultra = 3,

        // Not a real preset; means the individual fields don't match any preset
        // (the user has tweaked them manually).
        Custom = 4
    };

    struct Settings
    {
        QualityPreset Preset = QualityPreset::High;

        // --- Live settings (safe to change at runtime) ---

        bool Shadows = true;

        bool AmbientOcclusion = true;

        bool Bloom = true;

        // Number of hemisphere samples the SSAO shader takes per fragment.
        // Clamped to [1, 64] (the kernel buffer holds 64 entries).
        int AmbientOcclusionSamples = 24;

        // Number of blur passes applied to the SSAO result.
        int AmbientOcclusionBlurPasses = 4;

        // Number of blur passes applied to the extracted bright areas. More = a wider, softer glow.
        int BloomBlurPasses = 6;

        // How many shadow atlas tiles spot and point lights may hold at once. A spot light costs
        // one tile and a point light six, so below 6 point lights never cast. It caps how many
        // tiles can be re-rendered in one frame when everything moves, not memory.
        //
        // The Low preset turns shadows off, so its value here is only what Custom starts from.
        int LocalShadowTileBudget = 12;

        // --- Allocation-class settings (applied at Setup(), restart to change) ---

        // Shadow atlas resolution (square), and the one knob for shadow resolution. Every shadow in
        // the engine lives in it: at 4096 that is 2048 per cascade, 1024 for a nearby local light
        // and 512 for a distant one, in 32 MB of D16. The number of tiles does not depend on it.
        int ShadowAtlasResolution = 4096;

        // The SSAO buffer is rendered at (internal resolution / this divisor).
        // Higher = lower AO resolution = faster.
        int AmbientOcclusionResDivisor = 2;

        // The bloom buffers are rendered at (internal resolution / this divisor). Bloom is a wide,
        // soft blur, so low resolution is the point rather than only a saving - 1 is rarely worth it.
        int BloomResDivisor = 2;
    };

    // Loads "graphics.json" from the working directory. If missing, keeps the
    // default (High) preset. Must be called before RenderManager::Setup() so the
    // allocation-class values are available when GPU buffers are sized.
    void Setup();

    const Settings& Get();
    void Set(const Settings& settings);

    // Writes the current settings back to "graphics.json".
    void Save();

    // Overwrites the live+allocation fields with the values for the given preset
    // and sets Preset accordingly. Does not touch disk.
    void ApplyPreset(QualityPreset preset);

    // Pushes the live (non-allocation) settings into the render pipeline so they
    // take effect immediately. Safe to call at any time after RenderManager::Setup().
    void ApplyRuntime();

    // --- Buffer-sizing accessors, read by rendering features at their Setup() ---

    int GetShadowAtlasResolution();
    int GetLocalShadowTileBudget();
    int GetAmbientOcclusionResDivisor();
    int GetAmbientOcclusionBlurPasses();
    int GetAmbientOcclusionSamples();
    int GetBloomResDivisor();
    int GetBloomBlurPasses();
}
