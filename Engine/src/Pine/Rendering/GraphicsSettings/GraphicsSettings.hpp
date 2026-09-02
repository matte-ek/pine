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

        // Number of hemisphere samples the SSAO shader takes per fragment.
        // Clamped to [1, 64] (the kernel buffer holds 64 entries).
        int AmbientOcclusionSamples = 24;

        // Number of blur passes applied to the SSAO result.
        int AmbientOcclusionBlurPasses = 4;

        // --- Allocation-class settings (applied at Setup(), restart to change) ---

        // Directional shadow map resolution (square).
        int ShadowMapResolution = 4096;

        // The SSAO buffer is rendered at (internal resolution / this divisor).
        // Higher = lower AO resolution = faster.
        int AmbientOcclusionResDivisor = 2;
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

    int GetShadowMapResolution();
    int GetAmbientOcclusionResDivisor();
    int GetAmbientOcclusionBlurPasses();
    int GetAmbientOcclusionSamples();
}
