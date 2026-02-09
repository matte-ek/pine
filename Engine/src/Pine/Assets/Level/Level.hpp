#pragma once

#include "Pine/Assets/Blueprint/Blueprint.hpp"
#include "Pine/Assets/Asset/Asset.hpp"

namespace Pine
{
    class Texture3D;

    struct LevelSettings
    {
        AssetHandle<Texture3D> Skybox;

        Vector3f AmbientColor = Vector3f(0.05f, 0.05f, 0.05f);

        Vector4f FogColor = Vector4f(0.0f, 0.0f, 0.0f, 1.0f);
        float FogDistance = 30.f;
        float FogIntensity = 0.f;

        bool HasCamera = false;
        std::uint32_t CameraEntity = 0;
    };

    class Level : public Asset
    {
    private:
        std::vector<Blueprint*> m_Blueprints;

        LevelSettings m_LevelSettings;
    public:
        explicit Level();

        void CreateFromWorld();
        void Load();

        void ClearBlueprints();

        std::size_t GetBlueprintCount() const;

        LevelSettings& GetLevelSettings();

        bool LoadFromFile(AssetLoadStage stage) override;
        bool SaveToFile() override;

        void Dispose() override;
    };

}