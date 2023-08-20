#pragma once

#include "Pine/Assets/Blueprint/Blueprint.hpp"
#include "Pine/Assets/IAsset/IAsset.hpp"

namespace Pine
{
    class Texture3D;
}

namespace Pine
{

    struct LevelSettings
    {
        Texture3D* Skybox = nullptr;
        std::uint32_t CameraEntity = 0;
    };

    class Level : public IAsset
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