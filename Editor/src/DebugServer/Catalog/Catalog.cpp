#include "Catalog.hpp"

#include <algorithm>
#include <filesystem>
#include <vector>

#include "../Editing/Values/Values.hpp"

#include "Pine/Assets/Assets.hpp"
#include "Pine/Assets/Material/Material.hpp"
#include "Pine/Assets/Model/Model.hpp"
#include "Pine/Assets/Shader/Shader.hpp"
#include "Pine/Assets/Texture2D/Texture2D.hpp"
#include "Pine/Core/File/File.hpp"
#include "Pine/Core/Serialization/Dump/SerializationDump.hpp"
#include "Pine/Core/Serialization/Json/SerializationJson.hpp"
#include "Pine/Core/String/String.hpp"

namespace
{
    using nlohmann::json;
    using namespace Editor::DebugServer;
    namespace Values = Editing::Values;

    // Matched against AssetTypeToString rather than a second lookup table, so the two cannot drift
    // apart as asset types get added.
    bool TryParseAssetType(const std::string& name, Pine::AssetType& type)
    {
        const auto wanted = Pine::String::ToLower(name);

        for (int candidate = 1; candidate < static_cast<int>(Pine::AssetType::Count); candidate++)
        {
            const auto assetType = static_cast<Pine::AssetType>(candidate);

            if (Pine::String::ToLower(Pine::AssetTypeToString(assetType)) == wanted)
            {
                type = assetType;

                return true;
            }
        }

        return false;
    }

    std::string JoinAssetTypeNames()
    {
        std::string names;

        for (int candidate = 1; candidate < static_cast<int>(Pine::AssetType::Count); candidate++)
        {
            if (!names.empty())
            {
                names += ", ";
            }

            names += Pine::AssetTypeToString(static_cast<Pine::AssetType>(candidate));
        }

        return names;
    }

    const char* RenderingModeToString(const Pine::MaterialRenderingMode mode)
    {
        switch (mode)
        {
        case Pine::MaterialRenderingMode::Opaque:
            return "Opaque";
        case Pine::MaterialRenderingMode::Discard:
            return "Discard";
        case Pine::MaterialRenderingMode::Transparent:
            return "Transparent";
        default:
            return "Unknown";
        }
    }

    // Identity only - the four fields /assets lists for every asset, whatever its type.
    json ReadIdentity(const Pine::Asset* asset)
    {
        return {
            { "path", asset->GetPath() },
            { "type", Pine::AssetTypeToString(asset->GetType()) },
            { "uid", asset->GetUId().ToString() },
            { "modified", asset->HasBeenModified() }
        };
    }

    json ReadAssetReference(const Pine::Asset* asset)
    {
        if (asset == nullptr)
        {
            return nullptr;
        }

        return { { "id", asset->GetUId().ToString() }, { "path", asset->GetPath() } };
    }

    json StoreBounds(const Pine::Vector3f& minimum, const Pine::Vector3f& maximum)
    {
        return {
            { "min", Pine::SerializationJson::StoreVector3(minimum) },
            { "max", Pine::SerializationJson::StoreVector3(maximum) },
            { "size", Pine::SerializationJson::StoreVector3(maximum - minimum) }
        };
    }

    // The live model's own bounds and materials, rather than the stored ones GET /asset decompresses
    // from disk. That is the point of this route: it answers "how big is it and what is it made of"
    // for a shortlist of candidates without a file read per asset.
    void DescribeModel(const Pine::Model* model, json& entry)
    {
        entry["bounds"] = StoreBounds(model->GetBoundingBoxMin(), model->GetBoundingBoxMax());
        entry["meshCount"] = model->GetMeshes().size();

        std::uint64_t vertexCount = 0;
        std::vector<Pine::UId> seen;
        auto materials = json::array();

        for (const auto mesh : model->GetMeshes())
        {
            vertexCount += mesh->GetVertexCount();

            const auto material = mesh->GetMaterial();

            if (material == nullptr || std::find(seen.begin(), seen.end(), material->GetUId()) != seen.end())
            {
                continue;
            }

            seen.push_back(material->GetUId());
            materials.push_back(ReadAssetReference(material));
        }

        entry["vertexCount"] = vertexCount;
        entry["materials"] = materials;
    }

    void DescribeMaterial(const Pine::Material* material, json& entry)
    {
        entry["diffuseColor"] = Pine::SerializationJson::StoreVector3(material->GetDiffuseColor());
        entry["specularColor"] = Pine::SerializationJson::StoreVector3(material->GetSpecularColor());
        entry["renderingMode"] = RenderingModeToString(material->GetRenderingMode());
        entry["alpha"] = material->GetAlpha();
        entry["shininess"] = material->GetShininess();
        entry["shader"] = ReadAssetReference(material->GetShader());
        entry["textures"] = {
            { "diffuse", ReadAssetReference(material->GetDiffuse()) },
            { "specular", ReadAssetReference(material->GetSpecular()) },
            { "normal", ReadAssetReference(material->GetNormal()) }
        };
    }

    json Describe(Pine::Asset* asset)
    {
        auto entry = ReadIdentity(asset);

        if (const auto model = dynamic_cast<Pine::Model*>(asset))
        {
            DescribeModel(model, entry);
        }
        else if (const auto material = dynamic_cast<Pine::Material*>(asset))
        {
            DescribeMaterial(material, entry);
        }

        return entry;
    }

    // One entry of a summary request: exactly one of id or path, the same reference shape /edit
    // accepts for an asset property.
    Pine::Asset* ResolveReference(const json& value, const std::string& path)
    {
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

        return asset;
    }
}

Pine::Asset* Editor::DebugServer::Catalog::Resolve(const Request& request, Response& error)
{
    const auto pathParameter = request.Parameters.find("path");
    const auto idParameter = request.Parameters.find("id");

    if (pathParameter != request.Parameters.end())
    {
        const auto asset = Pine::Assets::GetAssetByPath(pathParameter->second);

        if (asset == nullptr)
        {
            error = Error(404, fmt::format("No asset at path '{}'.", pathParameter->second));
        }

        return asset;
    }

    if (idParameter != request.Parameters.end())
    {
        const Pine::UId id{ std::string(idParameter->second) };

        if (!id.IsValid())
        {
            error = Error(400, fmt::format(
                "'{}' is not a valid asset id. Ids look like '18d4ae6bff2fd8c6-213ebd6dcd8f0b73'.",
                idParameter->second));

            return nullptr;
        }

        const auto asset = Pine::Assets::GetAssetByUId(id);

        if (asset == nullptr)
        {
            error = Error(404, fmt::format("No asset with id '{}'.", idParameter->second));
        }

        return asset;
    }

    error = Error(400, "Expected a ?path= or ?id= parameter. /assets lists both.");

    return nullptr;
}

Editor::DebugServer::Response Editor::DebugServer::Catalog::List(const Request& request)
{
    const auto typeParameter = request.Parameters.find("type");

    bool filterByType = false;
    Pine::AssetType filterType = Pine::AssetType::Invalid;

    if (typeParameter != request.Parameters.end())
    {
        if (!TryParseAssetType(typeParameter->second, filterType))
        {
            return Error(400, fmt::format(
                "Unknown asset type '{}'. Expected one of: {}.", typeParameter->second, JoinAssetTypeNames()));
        }

        filterByType = true;
    }

    std::vector<const Pine::Asset*> assets;

    for (const auto& [uid, asset] : Pine::Assets::GetAll())
    {
        if (asset == nullptr)
        {
            continue;
        }

        if (filterByType && asset->GetType() != filterType)
        {
            continue;
        }

        assets.push_back(asset);
    }

    // GetAll() is an unordered_map, so without this the listing shuffles between calls - which
    // makes it useless for scanning or diffing.
    std::sort(assets.begin(), assets.end(), [](const Pine::Asset* left, const Pine::Asset* right)
    {
        return left->GetPath() < right->GetPath();
    });

    // Deliberately lightweight - path, type, identity. A project can hold thousands of these, so
    // anything more per asset belongs in /asset, or in /assets/summary for a shortlist.
    auto entries = json::array();

    for (const auto asset : assets)
    {
        entries.push_back(ReadIdentity(asset));
    }

    json body;

    body["count"] = entries.size();
    body["assets"] = entries;

    return { 200, body };
}

Editor::DebugServer::Response Editor::DebugServer::Catalog::Get(const Request& request)
{
    Response error;

    const auto asset = Resolve(request, error);

    if (asset == nullptr)
    {
        return error;
    }

    const auto& filePath = asset->GetFilePath();

    if (filePath.empty() || !std::filesystem::exists(filePath))
    {
        return Error(409, fmt::format("Asset '{}' has no file on disk to read.", asset->GetPath()));
    }

    // Read back from disk rather than re-serializing the live asset. Asset::Save() stamps a new
    // creation time, and a GET must not mutate what it reports on. So this is the *stored*
    // asset: "modified" tells you when the in-memory one has diverged, and the live world is
    // what /entities is for.
    const auto content = Pine::Serialization::Dump::ToJson(Pine::File::ReadCompressed(filePath));

    if (!content.has_value())
    {
        return Error(409, fmt::format("'{}' could not be read as a Pine serialized file.", filePath.string()));
    }

    auto body = ReadIdentity(asset);

    body["file"] = filePath.string();
    body["content"] = *content;

    return { 200, body };
}

Editor::DebugServer::Response Editor::DebugServer::Catalog::Summary(const Request& request)
{
    try
    {
        Values::Require(request.Body.size() <= 16 * 1024, "", "Summary request exceeds 16 KiB.");

        const auto depthLimit = [](int depth, json::parse_event_t, json&)
        {
            Values::Require(depth <= 8, "", "Summary JSON nesting exceeds 8 levels.");
            return true;
        };

        const auto body = json::parse(request.Body, depthLimit, false);

        Values::Require(!body.is_discarded(), "", "Request body is not valid JSON.");
        Values::Object(body, "", { "assets" }, { "assets" });

        const auto& references = body.at("assets");

        Values::Require(references.is_array() && !references.empty() && references.size() <= 256,
            "/assets", "Expected 1 to 256 asset references.");

        auto entries = json::array();

        for (std::size_t index = 0; index < references.size(); index++)
        {
            entries.push_back(Describe(ResolveReference(references[index], "/assets/" + std::to_string(index))));
        }

        return { 200, { { "count", entries.size() }, { "assets", entries } } };
    }
    catch (const Values::ValidationError& exception)
    {
        return { 400, { { "error", exception.what() }, { "path", exception.Path } } };
    }
}
