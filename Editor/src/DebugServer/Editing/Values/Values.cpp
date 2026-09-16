#include "Values.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

#include "Pine/Assets/Assets.hpp"

namespace
{
    using nlohmann::json;
    namespace Values = Editor::DebugServer::Editing::Values;

    double Number(const json& value, const std::string& path, const json& schema)
    {
        Values::Require(value.is_number(), path, "Expected a number.");
        const double number = value.get<double>();
        Values::Require(std::isfinite(number) && std::abs(number) <= std::numeric_limits<float>::max(),
            path, "Expected a finite number representable as float32.");

        if (schema.contains("minimum"))
        {
            Values::Require(number >= schema.at("minimum").get<double>(), path, "Value is below minimum.");
        }
        if (schema.contains("maximum"))
        {
            Values::Require(number <= schema.at("maximum").get<double>(), path, "Value is above maximum.");
        }

        return number;
    }

    void ValidateValue(json& value, const json& schema, const std::string& path)
    {
        const auto type = schema.at("type").get<std::string>();

        if (type == "number" || type == "integer")
        {
            if (type == "integer")
            {
                Values::Require(value.is_number_integer(), path, "Expected an integer.");
            }
            Number(value, path, schema);
        }
        else if (type == "string")
        {
            Values::String(value, path);
        }
        else if (type == "boolean")
        {
            Values::Require(value.is_boolean(), path, "Expected a boolean.");
        }
        else if (type == "boolean3")
        {
            Values::Object(value, path, { "x", "y", "z" }, { "x", "y", "z" });
            for (const auto& item : value.items())
            {
                Values::Require(item.value().is_boolean(), path + "/" + item.key(), "Expected a boolean.");
            }
        }
        else if (type == "enum")
        {
            Values::String(value, path);
            const auto& choices = schema.at("values");
            Values::Require(std::find(choices.begin(), choices.end(), value) != choices.end(),
                path, "Unknown enum value. See /edit/schema for supported names.");
        }
        else if (type == "vector3" || type == "quaternion")
        {
            if (type == "vector3")
            {
                Values::Object(value, path, { "x", "y", "z" }, { "x", "y", "z" });
            }
            else
            {
                Values::Object(value, path, { "x", "y", "z", "w" }, { "x", "y", "z", "w" });
            }

            double lengthSquared = 0;
            for (auto& item : value.items())
            {
                const double number = Number(item.value(), path + "/" + item.key(), schema);
                lengthSquared += number * number;
            }

            if (type == "quaternion")
            {
                Values::Require(lengthSquared > 0, path, "Quaternion must have nonzero length.");
                const double length = std::sqrt(lengthSquared);
                for (auto& item : value.items())
                {
                    item.value() = static_cast<float>(item.value().get<double>() / length);
                }
            }
        }
        else if (type == "asset")
        {
            if (value.is_null())
            {
                Values::Require(schema.value("nullable", false), path, "This asset reference cannot be null.");
                return;
            }

            Values::Object(value, path, { "id", "path" });
            Values::Require(value.size() == 1, path, "Expected exactly one of id or path.");

            Pine::Asset* asset = nullptr;
            if (value.contains("id"))
            {
                asset = Pine::Assets::GetAssetByUId(Values::Id(value.at("id"), path + "/id"));
            }
            else
            {
                asset = Pine::Assets::GetAssetByPath(Values::String(value.at("path"), path + "/path"));
            }

            Values::Require(asset != nullptr, path, "Asset is not loaded or does not exist.");
            Values::Require(Pine::AssetTypeToString(asset->GetType()) == schema.at("assetType").get<std::string>(),
                path, "Asset has the wrong type; expected " + schema.at("assetType").get<std::string>() + ".");
            value = Values::AssetReference(asset);
        }
        else
        {
            throw std::logic_error("Unknown editing schema type: " + type);
        }
    }
}

Values::ValidationError::ValidationError(const std::string& path, const std::string& message)
    : std::runtime_error(message), Path(path)
{
}

void Values::Require(const bool condition, const std::string& path, const std::string& message)
{
    if (!condition)
    {
        throw ValidationError(path, message);
    }
}

void Values::Object(const json& value, const std::string& path,
    std::initializer_list<const char*> allowed, std::initializer_list<const char*> required)
{
    Require(value.is_object(), path, "Expected an object.");

    for (const auto& item : value.items())
    {
        const auto found = std::find_if(allowed.begin(), allowed.end(), [&](const char* name)
        {
            return item.key() == name;
        });
        Require(found != allowed.end(), path + "/" + item.key(), "Unknown field.");
    }
    for (const auto name : required)
    {
        Require(value.contains(name), path + "/" + name, "Required field is missing.");
    }
}

std::string Values::String(const json& value, const std::string& path)
{
    Require(value.is_string(), path, "Expected a string.");
    const auto result = value.get<std::string>();
    Require(!result.empty() && result.find('\0') == std::string::npos, path,
        "Expected a nonempty string without null characters.");
    return result;
}

Pine::UId Values::Id(const json& value, const std::string& path)
{
    const auto text = String(value, path);
    const auto separator = text.find('-');
    Require(separator >= 1 && separator <= 16 && text.size() == separator + 17, path,
        "Expected a Pine UId: 1-16 hexadecimal digits, '-', then 16 hexadecimal digits.");

    for (std::size_t index = 0; index < text.size(); index++)
    {
        const char character = text[index];
        Require(index == separator || (character >= '0' && character <= '9') ||
            (character >= 'a' && character <= 'f') || (character >= 'A' && character <= 'F'),
            path, "UId contains a non-hexadecimal character.");
    }

    const Pine::UId id(text);
    Require(id.IsValid(), path, "Expected a nonempty UId.");
    return id;
}

void Values::Properties(json& state, const json& schema, const std::string& path)
{
    Require(state.is_object(), path, "Expected a property object.");
    for (auto& item : state.items())
    {
        Require(schema.contains(item.key()), path + "/" + item.key(), "Unknown property.");
        ValidateValue(item.value(), schema.at(item.key()), path + "/" + item.key());
    }
}

Pine::Vector3f Values::Vector3(const json& value)
{
    return { value.at("x").get<float>(), value.at("y").get<float>(), value.at("z").get<float>() };
}

Pine::Quaternion Values::Quaternion(const json& value)
{
    return { value.at("w").get<float>(), value.at("x").get<float>(),
        value.at("y").get<float>(), value.at("z").get<float>() };
}

json Values::AssetReference(const Pine::Asset* asset)
{
    if (asset == nullptr)
    {
        return nullptr;
    }
    return { { "id", asset->GetUId().ToString() } };
}

Pine::Asset* Values::ResolvedAsset(const json& value)
{
    if (value.is_null())
    {
        return nullptr;
    }
    return Pine::Assets::GetAssetByUId(Pine::UId(value.at("id").get<std::string>()));
}
