#pragma once
#include "Pine/Assets/IAsset/IAsset.hpp"
#include "Pine/World/Entity/Entity.hpp"
#include <type_traits>
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
        static_assert(std::is_same<T, Pine::Entity>::value || std::is_same<T, Pine::IAsset>::value);

        if (limitOne)
            Clear();

        if (std::is_same<T, Pine::Entity>::value)
        {
            AddEntity(reinterpret_cast<Pine::Entity*>(item));
        }
        else if (std::is_same<T, Pine::IAsset>::value)
        {
            AddAsset(reinterpret_cast<Pine::IAsset*>(item));
        }
    }

    template <typename T>
    bool IsSelected(T* item)
    {
        static_assert(std::is_same<T, Pine::Entity>::value || std::is_same<T, Pine::IAsset>::value);

        if (std::is_same<T, Pine::Entity>::value)
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
        else if (std::is_same<T, Pine::IAsset>::value)
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
    }
}