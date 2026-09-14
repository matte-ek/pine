#include "Endpoints.hpp"

#include <algorithm>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

#include "DebugServer/DebugServer.hpp"
#include "DebugServer/Screenshot/Screenshot.hpp"

#include "Gui/Shared/Selection/Selection.hpp"
#include "Other/PlayHandler/PlayHandler.hpp"
#include "Projects/Projects.hpp"
#include "Rendering/RenderHandler.hpp"

#include "Pine/Assets/Assets.hpp"
#include "Pine/Assets/Level/Level.hpp"
#include "Pine/Core/File/File.hpp"
#include "Pine/Engine/Engine.hpp"
#include "Pine/Core/Log/Log.hpp"
#include "Pine/Core/Serialization/Dump/SerializationDump.hpp"
#include "Pine/Core/String/String.hpp"
#include "Pine/Performance/Performance.hpp"
#include "Pine/Rendering/RenderManager/RenderManager.hpp"
#include "Pine/World/Entities/Entities.hpp"
#include "Pine/World/Entity/Entity.hpp"
#include "Pine/World/World.hpp"

namespace
{
    /* Shared helpers */

    // A parsed query parameter. Keeps "not given" apart from "given but nonsense", so a typo in
    // ?limit= answers with a 400 instead of quietly behaving like the default.
    struct IntParameter
    {
        bool Present = false;
        bool Valid = false;
        int Value = 0;
    };

    IntParameter ReadIntParameter(const Editor::DebugServer::Request& request, const std::string& name)
    {
        const auto parameter = request.Parameters.find(name);

        if (parameter == request.Parameters.end())
        {
            return {};
        }

        IntParameter result;

        result.Present = true;

        try
        {
            std::size_t consumed = 0;

            result.Value = std::stoi(parameter->second, &consumed);
            result.Valid = consumed == parameter->second.size();
        }
        catch (const std::exception&)
        {
            result.Valid = false;
        }

        return result;
    }

    const char* GameStateToString(const PlayHandler::EditorGameState state)
    {
        switch (state)
        {
        case PlayHandler::EditorGameState::Stopped:
            return "stopped";
        case PlayHandler::EditorGameState::Playing:
            return "playing";
        case PlayHandler::EditorGameState::Paused:
            return "paused";
        }

        return "unknown";
    }

    const char* LogSeverityToString(const Pine::LogSeverity severity)
    {
        switch (severity)
        {
        case Pine::LogSeverity::Verbose:
            return "verbose";
        case Pine::LogSeverity::Info:
            return "info";
        case Pine::LogSeverity::Warning:
            return "warning";
        case Pine::LogSeverity::Error:
            return "error";
        case Pine::LogSeverity::Fatal:
            return "fatal";
        }

        return "unknown";
    }

    /* GET /status */

    Editor::DebugServer::Response GetStatus(const Editor::DebugServer::Request&)
    {
        const auto activeLevel = Pine::World::GetActiveLevel();

        nlohmann::json body;

        body["project"] = Editor::Projects::GetProjectName();
        body["playState"] = GameStateToString(PlayHandler::GetGameState());
        body["worldPaused"] = Pine::World::IsPaused();
        body["entityCount"] = Pine::Entities::GetList().size();
        body["deltaTime"] = Pine::RenderManager::GetGlobalDeltaTime();

        if (activeLevel != nullptr)
        {
            body["activeLevel"] = activeLevel->GetPath();
        }
        else
        {
            body["activeLevel"] = nullptr;
        }

        return { 200, body };
    }

    /* GET /logs */

    Editor::DebugServer::Response GetLogs(const Editor::DebugServer::Request& request)
    {
        const auto limit = ReadIntParameter(request, "limit");

        if (limit.Present && (!limit.Valid || limit.Value < 1))
        {
            return Editor::DebugServer::Error(400, "Parameter 'limit' must be a positive integer.");
        }

        const auto& messages = Pine::Log::GetLogMessages();

        // The engine caps its buffer at 256, so returning all of it is a sane default. ?limit= takes
        // the most recent N instead, which is what you want right after reproducing something.
        std::size_t offset = 0;

        if (limit.Present && static_cast<std::size_t>(limit.Value) < messages.size())
        {
            offset = messages.size() - static_cast<std::size_t>(limit.Value);
        }

        nlohmann::json entries = nlohmann::json::array();

        for (auto message = messages.begin() + static_cast<long>(offset); message != messages.end(); ++message)
        {
            nlohmann::json entry;

            entry["severity"] = LogSeverityToString(message->Type);
            entry["message"] = message->Message;
            entry["file"] = message->FileName;
            entry["line"] = message->FileLine;

            entries.push_back(entry);
        }

        nlohmann::json body;

        body["totalBuffered"] = messages.size();
        body["messages"] = entries;

        return { 200, body };
    }

    /* GET /entities */

    // The fields the tree listing and the single-entity view both report, so the two cannot describe
    // the same entity differently.
    nlohmann::json StoreEntityIdentity(const Pine::Entity* entity)
    {
        nlohmann::json json;

        json["id"] = entity->GetId().ToString();
        json["internalId"] = entity->GetInternalId();
        json["name"] = entity->GetName();
        json["active"] = entity->GetActive();
        json["static"] = entity->GetStatic();

        // Editor-only entities (the fly camera) are temporary and are not part of the user's scene.
        json["temporary"] = entity->GetTemporary();

        return json;
    }

    nlohmann::json StoreEntity(const Pine::Entity* entity)
    {
        auto json = StoreEntityIdentity(entity);

        // Component *type names* only. Field values come from the serializer translator in phase 2,
        // so there is deliberately no hand-written per-component dump here to drift out of date.
        nlohmann::json components = nlohmann::json::array();

        for (const auto component : entity->GetComponents())
        {
            if (component == nullptr)
            {
                continue;
            }

            components.push_back(Pine::ComponentTypeToString(component->GetType()));
        }

        json["components"] = components;

        nlohmann::json children = nlohmann::json::array();

        for (const auto child : entity->GetChildren())
        {
            children.push_back(StoreEntity(child));
        }

        json["children"] = children;

        return json;
    }

    Editor::DebugServer::Response GetEntities(const Editor::DebugServer::Request&)
    {
        const auto& entities = Pine::Entities::GetList();

        // Walk from the roots and recurse, so the response mirrors the scene hierarchy. The engine's
        // list is flat and holds children too, hence the parent check rather than iterating it whole.
        nlohmann::json roots = nlohmann::json::array();

        for (const auto entity : entities)
        {
            if (entity == nullptr || entity->GetParent() != nullptr)
            {
                continue;
            }

            roots.push_back(StoreEntity(entity));
        }

        nlohmann::json body;

        body["count"] = entities.size();
        body["entities"] = roots;

        return { 200, body };
    }

    /* GET /stats */

    nlohmann::json StoreRenderingContext(const Pine::RenderingContext* context)
    {
        if (context == nullptr)
        {
            return nullptr;
        }

        const auto& statistics = context->Statistics;

        nlohmann::json json;

        json["active"] = context->Active;
        json["width"] = context->Size.x;
        json["height"] = context->Size.y;

        json["drawCalls"] = statistics.DrawCalls;
        json["vertexCount"] = statistics.VertexCount;
        json["lightCount"] = statistics.LightCount;
        json["visibleObjects"] = statistics.VisibleObjectCount;
        json["culledObjects"] = statistics.CulledObjectCount;
        json["renderTime"] = statistics.RenderTime;

        return json;
    }

    Editor::DebugServer::Response GetStats(const Editor::DebugServer::Request&)
    {
        nlohmann::json scopes = nlohmann::json::array();

        for (const auto scope : Pine::Performance::GetTrackedScopes())
        {
            nlohmann::json entry;

            entry["name"] = scope->Name;
            entry["time"] = scope->Time;

            scopes.push_back(entry);
        }

        nlohmann::json body;

        body["deltaTime"] = Pine::RenderManager::GetGlobalDeltaTime();
        body["level"] = StoreRenderingContext(Editor::RenderHandler::GetLevelRenderingContext());
        body["game"] = StoreRenderingContext(Editor::RenderHandler::GetGameRenderingContext());
        body["scopes"] = scopes;

        return { 200, body };
    }

    /* GET /entity */

    nlohmann::json StoreComponentDetail(Pine::Component* component)
    {
        nlohmann::json json;

        json["type"] = Pine::ComponentTypeToString(component->GetType());
        json["typeId"] = static_cast<int>(component->GetType());
        json["id"] = component->GetId().ToString();
        json["internalId"] = component->GetInternalId();
        json["active"] = component->GetActive();

        // The component's own serializer output, translated. Nothing here knows what a Light or a
        // Collider holds: whatever the component writes in SaveData() is what appears, so this view
        // cannot drift from the real serializer and needs no update when a component gains a field.
        //
        // Note this is serialized state, not every runtime member - Transform reports its local
        // position, not the world matrix it computes from it.
        const auto data = component->SaveData();
        const auto translated = Pine::Serialization::Dump::ToJson(data);

        json["data"] = translated.has_value() ? *translated : nlohmann::json(nullptr);

        return json;
    }

    Editor::DebugServer::Response GetEntity(const Editor::DebugServer::Request& request)
    {
        const auto idParameter = request.Parameters.find("id");
        const auto internalIdParameter = request.Parameters.find("internalId");

        Pine::Entity* entity = nullptr;

        if (idParameter != request.Parameters.end())
        {
            const Pine::UId id{ std::string(idParameter->second) };

            if (!id.IsValid())
            {
                return Editor::DebugServer::Error(400, fmt::format(
                    "'{}' is not a valid entity id. Ids look like '18d4ae6bff2fd8c6-213ebd6dcd8f0b73'.",
                    idParameter->second));
            }

            entity = Pine::Entities::Find(id);

            if (entity == nullptr)
            {
                return Editor::DebugServer::Error(404, fmt::format("No entity with id '{}'.", idParameter->second));
            }
        }
        else if (internalIdParameter != request.Parameters.end())
        {
            const auto internalId = ReadIntParameter(request, "internalId");

            // Bounded before the lookup on purpose: Entities::GetByInternalId only asserts the index
            // is in range, so an out-of-range value aborts a debug build and reads out of bounds in a
            // release one. A query parameter must not be able to do either.
            const auto maxEntityCount = static_cast<int>(Pine::Engine::GetEngineConfiguration().m_MaxObjectCount);

            if (!internalId.Valid || internalId.Value < 0 || internalId.Value >= maxEntityCount)
            {
                return Editor::DebugServer::Error(400, fmt::format(
                    "Parameter 'internalId' must be between 0 and {}.", maxEntityCount - 1));
            }

            entity = Pine::Entities::GetByInternalId(static_cast<std::uint32_t>(internalId.Value));

            if (entity == nullptr)
            {
                return Editor::DebugServer::Error(404, fmt::format(
                    "No entity occupying internalId {}.", internalId.Value));
            }
        }
        else
        {
            return Editor::DebugServer::Error(400, "Expected an ?id= or ?internalId= parameter. /entities lists both.");
        }

        auto body = StoreEntityIdentity(entity);

        body["tags"] = entity->GetTags();

        if (const auto parent = entity->GetParent())
        {
            body["parent"] = StoreEntityIdentity(parent);
        }
        else
        {
            body["parent"] = nullptr;
        }

        nlohmann::json components = nlohmann::json::array();

        for (const auto component : entity->GetComponents())
        {
            if (component == nullptr)
            {
                continue;
            }

            components.push_back(StoreComponentDetail(component));
        }

        body["components"] = components;

        // Children by identity only. /entities already gives the whole tree, and expanding every
        // descendant's fields here would make one request unbounded.
        nlohmann::json children = nlohmann::json::array();

        for (const auto child : entity->GetChildren())
        {
            children.push_back(StoreEntityIdentity(child));
        }

        body["children"] = children;

        return { 200, body };
    }

    /* GET /assets */

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

    Editor::DebugServer::Response GetAssets(const Editor::DebugServer::Request& request)
    {
        const auto typeParameter = request.Parameters.find("type");

        bool filterByType = false;
        Pine::AssetType filterType = Pine::AssetType::Invalid;

        if (typeParameter != request.Parameters.end())
        {
            if (!TryParseAssetType(typeParameter->second, filterType))
            {
                return Editor::DebugServer::Error(400, fmt::format(
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

        // Deliberately lightweight - path, type, identity. Anything more per asset belongs in the
        // per-asset endpoint, since a project can hold thousands of these.
        nlohmann::json entries = nlohmann::json::array();

        for (const auto asset : assets)
        {
            nlohmann::json entry;

            entry["path"] = asset->GetPath();
            entry["type"] = Pine::AssetTypeToString(asset->GetType());
            entry["uid"] = asset->GetUId().ToString();
            entry["modified"] = asset->HasBeenModified();

            entries.push_back(entry);
        }

        nlohmann::json body;

        body["count"] = entries.size();
        body["assets"] = entries;

        return { 200, body };
    }

    /* GET /asset */

    Editor::DebugServer::Response GetAsset(const Editor::DebugServer::Request& request)
    {
        const auto pathParameter = request.Parameters.find("path");
        const auto idParameter = request.Parameters.find("id");

        Pine::Asset* asset = nullptr;

        if (pathParameter != request.Parameters.end())
        {
            asset = Pine::Assets::GetAssetByPath(pathParameter->second);

            if (asset == nullptr)
            {
                return Editor::DebugServer::Error(404, fmt::format("No asset at path '{}'.", pathParameter->second));
            }
        }
        else if (idParameter != request.Parameters.end())
        {
            const Pine::UId id{ std::string(idParameter->second) };

            if (!id.IsValid())
            {
                return Editor::DebugServer::Error(400, fmt::format(
                    "'{}' is not a valid asset id. Ids look like '18d4ae6bff2fd8c6-213ebd6dcd8f0b73'.",
                    idParameter->second));
            }

            asset = Pine::Assets::GetAssetByUId(id);

            if (asset == nullptr)
            {
                return Editor::DebugServer::Error(404, fmt::format("No asset with id '{}'.", idParameter->second));
            }
        }
        else
        {
            return Editor::DebugServer::Error(400, "Expected a ?path= or ?id= parameter. /assets lists both.");
        }

        const auto& filePath = asset->GetFilePath();

        if (filePath.empty() || !std::filesystem::exists(filePath))
        {
            return Editor::DebugServer::Error(409, fmt::format(
                "Asset '{}' has no file on disk to read.", asset->GetPath()));
        }

        // Read back from disk rather than re-serializing the live asset. Asset::Save() stamps a new
        // creation time, and a GET must not mutate what it reports on. So this is the *stored*
        // asset: "modified" tells you when the in-memory one has diverged, and the live world is
        // what /entities is for.
        const auto content = Pine::Serialization::Dump::ToJson(Pine::File::ReadCompressed(filePath));

        if (!content.has_value())
        {
            return Editor::DebugServer::Error(409, fmt::format(
                "'{}' could not be read as a Pine serialized file.", filePath.string()));
        }

        nlohmann::json body;

        body["path"] = asset->GetPath();
        body["type"] = Pine::AssetTypeToString(asset->GetType());
        body["uid"] = asset->GetUId().ToString();
        body["modified"] = asset->HasBeenModified();
        body["file"] = filePath.string();
        body["content"] = *content;

        return { 200, body };
    }

    /* GET /viewport.png */

    Editor::DebugServer::Response GetViewportPng(const Editor::DebugServer::Request& request)
    {
        const auto viewParameter = request.Parameters.find("view");
        const auto view = viewParameter == request.Parameters.end() ? std::string("level") : viewParameter->second;

        const Pine::RenderingContext* context = nullptr;

        if (view == "level")
        {
            context = Editor::RenderHandler::GetLevelRenderingContext();
        }
        else if (view == "game")
        {
            context = Editor::RenderHandler::GetGameRenderingContext();
        }
        else
        {
            return Editor::DebugServer::Error(400,
                fmt::format("Unknown view '{}'. Expected 'level' or 'game'.", view));
        }

        const auto width = ReadIntParameter(request, "width");

        if (width.Present && (!width.Valid || width.Value < 1 || width.Value > 4096))
        {
            return Editor::DebugServer::Error(400, "Parameter 'width' must be between 1 and 4096.");
        }

        // An inactive viewport has a stale or zero-sized buffer. Saying so is far more useful than
        // returning a black image that looks like a broken capture.
        if (!context->Active)
        {
            return Editor::DebugServer::Error(409, fmt::format(
                "The {} viewport is not being rendered - its panel is hidden or not the selected tab.", view));
        }

        std::vector<std::uint8_t> png;

        if (!Editor::DebugServer::Screenshot::CapturePng(context, width.Present ? width.Value : 0, png))
        {
            return Editor::DebugServer::Error(409, fmt::format(
                "Nothing to capture from the {} viewport ({}x{}).",
                view, static_cast<int>(context->Size.x), static_cast<int>(context->Size.y)));
        }

        Editor::DebugServer::Response response;

        response.StatusCode = 200;
        response.Binary = std::move(png);
        response.ContentType = "image/png";

        return response;
    }

    /* POST /level/load */

    Editor::DebugServer::Response PostLevelLoad(const Editor::DebugServer::Request& request)
    {
        // Loading replaces every entity in the world. While playing or paused, PlayHandler is
        // holding a snapshot that Stop() restores, so swapping the level out from under it would
        // restore that snapshot into a world it no longer describes. Only Stopped is safe.
        if (PlayHandler::GetGameState() != PlayHandler::EditorGameState::Stopped)
        {
            return Editor::DebugServer::Error(409, fmt::format(
                "Cannot load a level while the editor is {}. Stop play mode first.",
                GameStateToString(PlayHandler::GetGameState())));
        }

        // The path may arrive as a JSON body or as ?path=. Reaching for curl with a query string is
        // a lot less friction than composing a body, and both are cheap to accept.
        std::string path;

        if (!request.Body.empty())
        {
            const auto body = nlohmann::json::parse(request.Body, nullptr, false);

            if (body.is_discarded())
            {
                return Editor::DebugServer::Error(400, "Request body is not valid JSON.");
            }

            if (!body.contains("path") || !body["path"].is_string())
            {
                return Editor::DebugServer::Error(400, "JSON body must contain a string \"path\".");
            }

            path = body["path"].get<std::string>();
        }
        else if (const auto parameter = request.Parameters.find("path"); parameter != request.Parameters.end())
        {
            path = parameter->second;
        }

        if (path.empty())
        {
            return Editor::DebugServer::Error(400,
                "Expected a level path, as a JSON body {\"path\": \"...\"} or a ?path= parameter.");
        }

        // Looked up as a plain asset first, so "no such path" and "that path is not a level" stay
        // distinguishable - Assets::Get<Level> collapses both to nullptr.
        const auto asset = Pine::Assets::GetAssetByPath(path);

        if (asset == nullptr)
        {
            return Editor::DebugServer::Error(404, fmt::format("No asset at path '{}'.", path));
        }

        const auto level = dynamic_cast<Pine::Level*>(asset);

        if (level == nullptr)
        {
            return Editor::DebugServer::Error(400, fmt::format("Asset '{}' is a {}, not a Level.",
                path, Pine::AssetTypeToString(asset->GetType())));
        }

        // Level::Load() deletes every entity, and Selection holds raw Entity pointers (see its own
        // TODO), so anything selected would dangle. PlayHandler::Stop() clears it for this reason.
        Selection::Clear();

        Pine::World::SetActiveLevel(level);

        nlohmann::json body;

        body["loaded"] = level->GetPath();
        body["entityCount"] = Pine::Entities::GetList().size();

        return { 200, body };
    }
}

void Editor::DebugServer::Endpoints::Register()
{
    AddRoute(Method::Get, "/status", GetStatus);
    AddRoute(Method::Get, "/logs", GetLogs);
    AddRoute(Method::Get, "/entities", GetEntities);
    AddRoute(Method::Get, "/entity", GetEntity);
    AddRoute(Method::Get, "/stats", GetStats);
    AddRoute(Method::Get, "/assets", GetAssets);
    AddRoute(Method::Get, "/asset", GetAsset);
    AddRoute(Method::Get, "/viewport.png", GetViewportPng);

    AddRoute(Method::Post, "/level/load", PostLevelLoad);
}
