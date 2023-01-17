#include "Serialization.hpp"
#include "Pine/Core/Log/Log.hpp"
#include <fstream>

std::optional<nlohmann::json> Pine::Serialization::LoadFromFile(const std::filesystem::path& path)
{
    if (!std::filesystem::exists(path))
        return {};

    std::ifstream fileStream(path);

    if (!fileStream.is_open())
        return {};

    nlohmann::json j;

    try
    {
        fileStream >> j;
    }
    catch (std::exception& e)
    {
        Log::Error("Error loading JSON file, " + path.filename().string());
        return {};
    }

    return j;
}

void Pine::Serialization::SaveToFile(const std::filesystem::path& path, const nlohmann::json& json)
{
    std::ofstream stream(path);

    stream << json;

    stream.close();
}

nlohmann::json Pine::Serialization::StoreVector2(const Pine::Vector3f& vector)
{
    nlohmann::json j;

    j["x"] = vector.x;
    j["y"] = vector.y;

    return j;
}

nlohmann::json Pine::Serialization::StoreVector3(const Pine::Vector3f& vector)
{
    nlohmann::json j;

    j["x"] = vector.x;
    j["y"] = vector.y;
    j["z"] = vector.z;

    return j;
}

nlohmann::json Pine::Serialization::StoreVector4(const Pine::Vector4f& vector)
{
    nlohmann::json j;

    j["x"] = vector.x;
    j["y"] = vector.y;
    j["z"] = vector.z;
    j["w"] = vector.w;

    return j;
}

nlohmann::json Pine::Serialization::StoreQuaternion(const Pine::Quaternion& quaternion)
{
    nlohmann::json j;

    j["x"] = quaternion.x;
    j["y"] = quaternion.y;
    j["z"] = quaternion.z;
    j["w"] = quaternion.w;

    return j;
}

void Pine::Serialization::LoadVector4(const nlohmann::json& j, const std::string& name, Pine::Vector4f& vec)
{
    if (!j.contains(name))
        return;

    vec.x = j[name]["x"].get<float>();
    vec.y = j[name]["y"].get<float>();
    vec.z = j[name]["z"].get<float>();
    vec.w = j[name]["w"].get<float>();
}

void Pine::Serialization::LoadVector3(const nlohmann::json& j, const std::string& name, Pine::Vector3f& vec)
{
    if (!j.contains(name))
        return;

    vec.x = j[name]["x"].get<float>();
    vec.y = j[name]["y"].get<float>();
    vec.z = j[name]["z"].get<float>();
}

void Pine::Serialization::LoadVector2(const nlohmann::json& j, const std::string& name, Pine::Vector2f& vec)
{
    if (!j.contains(name))
        return;

    vec.x = j[name]["x"].get<float>();
    vec.y = j[name]["y"].get<float>();
}

void Pine::Serialization::LoadQuaternion(const nlohmann::json& j, const std::string& name, Pine::Quaternion& quaternion)
{
    if (!j.contains(name))
        return;

    quaternion.x = j[name]["x"].get<float>();
    quaternion.y = j[name]["y"].get<float>();
    quaternion.z = j[name]["z"].get<float>();
    quaternion.w = j[name]["w"].get<float>();
}

nlohmann::json Pine::Serialization::StoreAsset(Pine::IAsset* asset)
{
    return asset == nullptr ? "null" : asset->GetPath();
}