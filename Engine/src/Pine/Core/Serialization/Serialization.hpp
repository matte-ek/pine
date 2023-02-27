#pragma once
#include "Pine/Assets/Assets.hpp"
#include "Pine/Assets/IAsset/IAsset.hpp"
#include "Pine/Core/Math/Math.hpp"
#include <filesystem>
#include <nlohmann/json.hpp>
#include <optional>
#include <string>

namespace Pine
{
    class IAsset;
}

namespace Pine::Serialization
{

    /* General */

    std::optional<nlohmann::json> LoadFromFile(const std::filesystem::path& path);
    void SaveToFile(const std::filesystem::path& path, const nlohmann::json& json);

    /* Store */

    nlohmann::json StoreVector2(const Vector3f& vector);
    nlohmann::json StoreVector3(const Vector3f& vector);
    nlohmann::json StoreVector4(const Vector4f& vector);

    nlohmann::json StoreQuaternion(const Quaternion& quaternion);

    nlohmann::json StoreAsset(const IAsset* asset);

    /* Load */

    void LoadVector2(const nlohmann::json& j, const std::string& name, Vector2f& vec);
    void LoadVector3(const nlohmann::json& j, const std::string& name, Vector3f& vec);
    void LoadVector4(const nlohmann::json& j, const std::string& name, Vector4f& vec);

    void LoadQuaternion(const nlohmann::json& j, const std::string& name, Quaternion& quaternion);

    template <typename T>
    void LoadAsset(const nlohmann::json& j, const std::string& name, Pine::AssetContainer<T>& asset, bool allowReference = true)
    {
        if (!j.contains(name))
            return;
        if (j[name] == "null")
            return;

        if (Pine::Assets::GetState() == AssetManagerState::LoadDirectory && allowReference)
        {
            Pine::Assets::AddAssetResolveReference({name, reinterpret_cast<AssetContainer<IAsset>*>(&asset)});
        }
        else
        {
            asset = Pine::Assets::GetAsset(j[name]);
        }
    }

    // Quick and easy way to load any data, but only if it exists. Generally makes the code look cleaner
    // within the component code.
    template <typename T>
    void LoadValue(const nlohmann::json& j, const std::string& name, T& value)
    {
        if (!j.contains(name))
            return;

        value = j[name];
    }

}