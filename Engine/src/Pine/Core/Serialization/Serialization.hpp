#pragma once
#include "Pine/Core/Math/Math.hpp"
#include <filesystem>
#include <nlohmann/json.hpp>
#include <optional>
#include <string>

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

    /* Load */

    void LoadVector2(const nlohmann::json& j, const std::string& name, Vector2f& vec);
    void LoadVector3(const nlohmann::json& j, const std::string& name, Vector3f& vec);
    void LoadVector4(const nlohmann::json& j, const std::string& name, Vector4f& vec);

    void LoadQuaternion(const nlohmann::json& j, const std::string& name, Quaternion& quaternion);


}