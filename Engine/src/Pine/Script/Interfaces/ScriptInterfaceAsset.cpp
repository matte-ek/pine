#include "Interfaces.hpp"
#include "Pine/Assets/Assets.hpp"
#include "Pine/Assets/AudioFile/AudioFile.hpp"
#include "Pine/Assets/Blueprint/Blueprint.hpp"
#include "Pine/World/Entities/Entities.hpp"
#include "Pine/Assets/Level/Level.hpp"
#include "Pine/Script/Bindings/Bindings.hpp"
#include "Pine/Script/Factory/ScriptObjectFactory.hpp"
#include "Pine/World/World.hpp"

namespace
{
    const char* GetFileName(Pine::UId id)
    {
        const auto asset = Pine::Assets::GetAssetByUId(id);
        if (!asset) return nullptr;
        return Pine::Script::Bindings::ReturnString(asset->GetFileName());
    }

    const char* GetPath(Pine::UId id)
    {
        const auto asset = Pine::Assets::GetAssetByUId(id);
        if (!asset) return nullptr;
        return Pine::Script::Bindings::ReturnString(asset->GetPath());
    }

    std::uint64_t GetByPath(const char* path)
    {
        const auto asset = Pine::Assets::Get<Pine::Asset>(path);

        if (!asset)
        {
            return 0;
        }

        return asset->GetScriptHandle()->Id;
    }

    std::uint64_t GetById(Pine::UId id)
    {
        const auto asset = Pine::Assets::GetAssetByUId(id);

        if (!asset)
        {
            return 0;
        }

        return asset->GetScriptHandle()->Id;
    }

    // -----------------------------------------------------

    float AudioGetDuration(Pine::UId id)
    {
        const auto audioFile = dynamic_cast<Pine::AudioFile*>(Pine::Assets::GetAssetByUId(id));
        if (!audioFile) return 0.f;
        return audioFile->GetDuration();
    }

    // -----------------------------------------------------

    std::uint64_t LevelGetActive()
    {
        const auto level = Pine::World::GetActiveLevel();

        if (!level)
        {
            return 0;
        }

        return level->GetScriptHandle()->Id;
    }

    // The active level is checked before the asset registry: the untitled level an editor session
    // starts with was never registered, so Assets::GetAssetByUId cannot find it.
    Pine::LevelSettings* GetLevelSettings(Pine::UId id)
    {
        auto level = Pine::World::GetActiveLevel();

        if (level == nullptr || !(level->GetUId() == id))
        {
            level = dynamic_cast<Pine::Level*>(Pine::Assets::GetAssetByUId(id));
        }

        if (!level) return nullptr;
        return &level->GetLevelSettings();
    }

    // The setters below clamp to the same minimums the editor's debug server enforces for these
    // settings, so a script cannot push a level into a state the editor would refuse.

    void LevelGetAmbientColor(Pine::UId id, Pine::Vector3f* color)
    {
        const auto settings = GetLevelSettings(id);
        if (!settings) return;
        *color = settings->AmbientColor;
    }

    void LevelSetAmbientColor(Pine::UId id, const Pine::Vector3f* color)
    {
        const auto settings = GetLevelSettings(id);
        if (!settings) return;
        settings->AmbientColor = glm::max(*color, Pine::Vector3f(0.f));
    }

    void LevelGetFogColor(Pine::UId id, Pine::Vector4f* color)
    {
        const auto settings = GetLevelSettings(id);
        if (!settings) return;
        *color = settings->FogColor;
    }

    void LevelSetFogColor(Pine::UId id, const Pine::Vector4f* color)
    {
        const auto settings = GetLevelSettings(id);
        if (!settings) return;
        settings->FogColor = glm::max(*color, Pine::Vector4f(0.f));
    }

    float LevelGetFogDistance(Pine::UId id)
    {
        const auto settings = GetLevelSettings(id);
        if (!settings) return 0.f;
        return settings->FogDistance;
    }

    void LevelSetFogDistance(Pine::UId id, float distance)
    {
        const auto settings = GetLevelSettings(id);
        if (!settings) return;
        settings->FogDistance = std::max(distance, 0.01f);
    }

    float LevelGetFogIntensity(Pine::UId id)
    {
        const auto settings = GetLevelSettings(id);
        if (!settings) return 0.f;
        return settings->FogIntensity;
    }

    void LevelSetFogIntensity(Pine::UId id, float intensity)
    {
        const auto settings = GetLevelSettings(id);
        if (!settings) return;
        settings->FogIntensity = std::max(intensity, 0.f);
    }

    float LevelGetExposure(Pine::UId id)
    {
        const auto settings = GetLevelSettings(id);
        if (!settings) return 0.f;
        return settings->Exposure;
    }

    void LevelSetExposure(Pine::UId id, float exposure)
    {
        const auto settings = GetLevelSettings(id);
        if (!settings) return;
        settings->Exposure = std::max(exposure, 0.f);
    }

    float LevelGetBloomThreshold(Pine::UId id)
    {
        const auto settings = GetLevelSettings(id);
        if (!settings) return 0.f;
        return settings->BloomThreshold;
    }

    void LevelSetBloomThreshold(Pine::UId id, float threshold)
    {
        const auto settings = GetLevelSettings(id);
        if (!settings) return;
        settings->BloomThreshold = std::max(threshold, 0.f);
    }

    float LevelGetBloomIntensity(Pine::UId id)
    {
        const auto settings = GetLevelSettings(id);
        if (!settings) return 0.f;
        return settings->BloomIntensity;
    }

    void LevelSetBloomIntensity(Pine::UId id, float intensity)
    {
        const auto settings = GetLevelSettings(id);
        if (!settings) return;
        settings->BloomIntensity = std::max(intensity, 0.f);
    }

    float LevelGetGrainStrength(Pine::UId id)
    {
        const auto settings = GetLevelSettings(id);
        if (!settings) return 0.f;
        return settings->GrainStrength;
    }

    void LevelSetGrainStrength(Pine::UId id, float strength)
    {
        const auto settings = GetLevelSettings(id);
        if (!settings) return;
        settings->GrainStrength = std::max(strength, 0.f);
    }

    float LevelGetVignetteStrength(Pine::UId id)
    {
        const auto settings = GetLevelSettings(id);
        if (!settings) return 0.f;
        return settings->VignetteStrength;
    }

    void LevelSetVignetteStrength(Pine::UId id, float strength)
    {
        const auto settings = GetLevelSettings(id);
        if (!settings) return;
        settings->VignetteStrength = std::max(strength, 0.f);
    }

    // -----------------------------------------------------

    // NOTE: The Blueprint/Level script interface below is still dormant — it predates the UId
    // migration and was never re-wired. Signatures now take the asset UId (so the managed side
    // binds correctly and resolution would go through Assets::GetAssetByUId), but the bodies
    // remain stubbed pending a separate pass to revive Blueprint/Level scripting.
    bool GetHasEntity(Pine::UId id)
    {
        return false;
        //return dynamic_cast<Pine::Blueprint*>(Pine::Assets::GetAssetByUId(id))->HasEntity();
    }

    void CreateFromEntity(Pine::UId id, std::uint32_t entityId)
    {
        //auto asset = dynamic_cast<Pine::Blueprint*>(Pine::Assets::GetAssetByUId(id));
        //asset->CreateFromEntity(Pine::Entities::GetByInternalId(entityId));
    }

    std::uint64_t SpawnEntity(Pine::UId id)
    {
        //return dynamic_cast<Pine::Blueprint*>(Pine::Assets::GetAssetByUId(id))->Spawn()->GetScriptHandle()->Id;
        return 0;
    }

    void LevelCreateFromWorld(Pine::UId id)
    {
        //auto asset = dynamic_cast<Pine::Level*>(Pine::Assets::GetAssetByUId(id));

        //asset->CreateFromWorld();
    }

    void LevelLoad(Pine::UId id)
    {
        //auto asset = dynamic_cast<Pine::Level*>(Pine::Assets::GetAssetByUId(id));

        //asset->Load();
    }
}

void Pine::Script::Interfaces::Asset::Setup()
{
    Bindings::Register("Pine.Assets.Asset::GetFileName", GetFileName);
    Bindings::Register("Pine.Assets.Asset::GetPath", GetPath);
    Bindings::Register("Pine.Assets.AssetManager::GetByPath", GetByPath);
    Bindings::Register("Pine.Assets.AssetManager::GetById", GetById);
    Bindings::Register("Pine.Assets.Audio::GetDuration", AudioGetDuration);
    Bindings::Register("Pine.Assets.Blueprint::GetHasEntity", GetHasEntity);
    Bindings::Register("Pine.Assets.Blueprint::CreateFromEntity", CreateFromEntity);
    Bindings::Register("Pine.Assets.Blueprint::SpawnEntity", SpawnEntity);
    Bindings::Register("Pine.Assets.Level::CreateFromWorld", LevelCreateFromWorld);
    Bindings::Register("Pine.Assets.Level::Load", LevelLoad);
    Bindings::Register("Pine.Assets.Level::GetActive", LevelGetActive);
    Bindings::Register("Pine.Assets.LevelRenderingSettings::GetAmbientColor", LevelGetAmbientColor);
    Bindings::Register("Pine.Assets.LevelRenderingSettings::SetAmbientColor", LevelSetAmbientColor);
    Bindings::Register("Pine.Assets.LevelRenderingSettings::GetFogColor", LevelGetFogColor);
    Bindings::Register("Pine.Assets.LevelRenderingSettings::SetFogColor", LevelSetFogColor);
    Bindings::Register("Pine.Assets.LevelRenderingSettings::GetFogDistance", LevelGetFogDistance);
    Bindings::Register("Pine.Assets.LevelRenderingSettings::SetFogDistance", LevelSetFogDistance);
    Bindings::Register("Pine.Assets.LevelRenderingSettings::GetFogIntensity", LevelGetFogIntensity);
    Bindings::Register("Pine.Assets.LevelRenderingSettings::SetFogIntensity", LevelSetFogIntensity);
    Bindings::Register("Pine.Assets.LevelRenderingSettings::GetExposure", LevelGetExposure);
    Bindings::Register("Pine.Assets.LevelRenderingSettings::SetExposure", LevelSetExposure);
    Bindings::Register("Pine.Assets.LevelRenderingSettings::GetBloomThreshold", LevelGetBloomThreshold);
    Bindings::Register("Pine.Assets.LevelRenderingSettings::SetBloomThreshold", LevelSetBloomThreshold);
    Bindings::Register("Pine.Assets.LevelRenderingSettings::GetBloomIntensity", LevelGetBloomIntensity);
    Bindings::Register("Pine.Assets.LevelRenderingSettings::SetBloomIntensity", LevelSetBloomIntensity);
    Bindings::Register("Pine.Assets.LevelRenderingSettings::GetGrainStrength", LevelGetGrainStrength);
    Bindings::Register("Pine.Assets.LevelRenderingSettings::SetGrainStrength", LevelSetGrainStrength);
    Bindings::Register("Pine.Assets.LevelRenderingSettings::GetVignetteStrength", LevelGetVignetteStrength);
    Bindings::Register("Pine.Assets.LevelRenderingSettings::SetVignetteStrength", LevelSetVignetteStrength);
}