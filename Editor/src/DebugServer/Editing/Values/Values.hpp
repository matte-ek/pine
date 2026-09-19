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
    Pine::Vector4f Vector4(const nlohmann::json& value);
    Pine::Quaternion Quaternion(const nlohmann::json& value);

    // Validates one vector3 field on its own, for the routes that take a direction or a target
    // outside a property object.
    Pine::Vector3f Vector3Field(const nlohmann::json& value, const std::string& path);

    // The rotation that looks along `direction`, rolled by `up`. `path` names the field the
    // direction came from. When the two are parallel an explicit up is rejected and an implied one
    // falls back, so a straight-down view still gets a stable orientation.
    Pine::Quaternion LookRotation(glm::dvec3 direction, glm::dvec3 up, const std::string& path, bool explicitUp);
    nlohmann::json AssetReference(const Pine::Asset* asset);
    Pine::Asset* ResolvedAsset(const nlohmann::json& value);
}
