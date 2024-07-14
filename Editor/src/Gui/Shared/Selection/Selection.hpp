#pragma once
#include "Pine/Assets/IAsset/IAsset.hpp"
#include "Pine/World/Entity/Entity.hpp"
#include <vector>

// You may select multiple items at once, however only one type at once.
// The selection system will make sure that two types won't be selected at once.

// TODO: Use handles instead of raw pointers, lol.

namespace Selection
{
    void AddEntity(Pine::Entity* entity);
    void AddAsset(Pine::IAsset* asset);

    void Clear();

    const std::vector<Pine::Entity*>& GetSelectedEntities();
    const std::vector<Pine::IAsset*>& GetSelectedAssets();

    template <typename T>
    void Add(T* item, bool limitOne = false)
    {
        static_assert(std::is_same_v<T, Pine::Entity> || std::is_same_v<T, Pine::IAsset>);

        if (limitOne)
            Clear();

        if (std::is_same_v<T, Pine::Entity>)
        {
            AddEntity(reinterpret_cast<Pine::Entity*>(item));
        }
        else if (std::is_same_v<T, Pine::IAsset>)
        {
            AddAsset(reinterpret_cast<Pine::IAsset*>(item));
        }
    }

    template <typename T>
    bool IsSelected(T* item)
    {
        static_assert(std::is_same_v<T, Pine::Entity> || std::is_same_v<T, Pine::IAsset>);

        if (std::is_same_v<T, Pine::Entity>)
        {
            for (auto entity : GetSelectedEntities())
            {
                if (entity == reinterpret_cast<Pine::Entity*>(item))
                {
                    return true;
                }
            }

            return false;
        }
        else if (std::is_same_v<T, Pine::IAsset>)
        {
            for (auto asset : GetSelectedAssets())
            {
                if (asset == reinterpret_cast<Pine::IAsset*>(item))
                {
                    return true;
                }
            }

            return false;
        }

        return false;
    }
}