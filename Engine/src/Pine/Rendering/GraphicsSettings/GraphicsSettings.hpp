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

        // How many shadow atlas tiles local (spot, and later point) lights may hold at once.
        //
        // A tile budget rather than per-light-type counts: a scene with five spots and no point
        // lights should just work, and "2 point + 1 spot" cannot say that. A point light will simply
        // cost six of these.
        //
        // It bounds worst-case cost, not memory. Holding a tile whose contents are still valid is
        // nearly free - the saving caching buys - so the number that hurts is how many tiles can be
        // *re-rendered* in one frame, and that is what this caps in the bad case where they all move
        // at once.
        //
        // A point light costs six of these, so anything under 6 silently means "point lights never
        // cast" - which is why the presets step 6 / 12 / 16 rather than 2 / 4 / 8.
        int LocalShadowTileBudget = 12;

        // --- Allocation-class settings (applied at Setup(), restart to change) ---

        // Shadow atlas resolution (square). Every shadow in the engine lives in it: the directional
        // cascades pin the two half-size tiles, spot and point lights compete for the rest.
        //
        // This is the single knob for shadow quality now. At 4096 that is 2048 per cascade, 1024 for
        // a nearby local light and 512 for a distant one, in 32 MB of D16 - against the 142 MB the
        // separate 4096x4096x2 cascade array and a 2048 atlas used to cost between them.
        //
        // Being generous costs memory but not frame time: with per-tile scissored clears and
        // caching, an atlas where nothing moved re-renders nothing regardless of its size. Being
        // frugal actively hurts - too small and the allocator starts evicting, and a light
        // oscillating on the eviction boundary re-renders every frame, which is the exact case
        // caching exists to prevent.
        int ShadowAtlasResolution = 4096;

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

    int GetShadowAtlasResolution();
    int GetLocalShadowTileBudget();
    int GetAmbientOcclusionResDivisor();
    int GetAmbientOcclusionBlurPasses();
    int GetAmbientOcclusionSamples();
}
