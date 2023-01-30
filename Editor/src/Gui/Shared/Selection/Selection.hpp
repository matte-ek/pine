#pragma once
#include "Pine/Assets/IAsset/IAsset.hpp"
#include "Pine/World/Entity/Entity.hpp"
#include <type_traits>

namespace Selection
{
    void SelectEntity(Pine::Entity* entity);
    void SelectAsset(Pine::IAsset* asset);

    void Clear();

    Pine::Entity* GetSelectedEntity();
    Pine::IAsset* GetSelectedAsset();

    template <typename T>
    void Select(T* item)
    {
        static_assert(std::is_same<T, Pine::Entity>::value || std::is_same<T, Pine::IAsset*>::value);

        if (std::is_same<T, Pine::Entity>::value)
        {
            SelectEntity(reinterpret_cast<Pine::Entity*>(item));
        }
        else if (std::is_same<T, Pine::IAsset>::value)
        {
            SelectAsset(reinterpret_cast<Pine::IAsset*>(item));
        }
    }

    template <typename T>
    T* Get()
    {
        static_assert(std::is_same<T, Pine::Entity>::value || std::is_same<T, Pine::IAsset>::value);

        if (std::is_same<T, Pine::Entity>::value)
        {
            return reinterpret_cast<T*>(GetSelectedEntity());
        }
        else if (std::is_same<T, Pine::IAsset>::value)
        {
            return reinterpret_cast<T*>(GetSelectedEntity());
        }
    }
}