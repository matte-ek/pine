#pragma once

#include "Pine/Assets/Blueprint/Blueprint.hpp"
#include "Pine/Assets/IAsset/IAsset.hpp"
namespace Pine
{

    class Level : public IAsset
    {
    private:
        std::vector<Blueprint*> m_Blueprints;
    public:
        explicit Level();

        void CreateFromWorld();
        void Load() const;

        void ClearBlueprints();

        std::size_t GetBlueprintCount() const;

        bool LoadFromFile(AssetLoadStage stage) override;
        bool SaveToFile() override;

        void Dispose() override;
    };

}