#pragma once

#include <initializer_list>
#include <stdexcept>
#include <string>

#include <nlohmann/json.hpp>

#include "Pine/Assets/Asset/Asset.hpp"
#include "Pine/Core/Math/Math.hpp"

namespace Editor::DebugServer::Editing::Values
{
    class ValidationError : public std::runtime_error
    {
    public:
        std::string Path;

        ValidationError(const std::string& path, const std::string& message);
    };

    void Require(bool condition, const std::string& path, const std::string& message);
    void Object(const nlohmann::json& value, const std::string& path,
        std::initializer_list<const char*> allowed, std::initializer_list<const char*> required = {});
    std::string String(const nlohmann::json& value, const std::string& path);
    Pine::UId Id(const nlohmann::json& value, const std::string& path);

    // Validates and normalizes supplied properties using the advertised schema.
    // Asset paths become IDs and quaternions become unit quaternions before execution.
    void Properties(nlohmann::json& state, const nlohmann::json& schema, const std::string& path);

    Pine::Vector3f Vector3(const nlohmann::json& value);
    Pine::Quaternion Quaternion(const nlohmann::json& value);
    nlohmann::json AssetReference(const Pine::Asset* asset);
    Pine::Asset* ResolvedAsset(const nlohmann::json& value);
}
