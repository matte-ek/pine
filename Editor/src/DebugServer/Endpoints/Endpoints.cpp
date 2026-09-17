#include "Endpoints.hpp"
#include "../Camera/Camera.hpp"
#include "../Spatial/Spatial.hpp"
#include "../Editing/Editing.hpp"
#include "../Editing/History/History.hpp"
#include "../Observation/Observation.hpp"
#include "../Picking/Picking.hpp"
#include "../LogHistory/LogHistory.hpp"
#include "../Persistence/Persistence.hpp"
#include "../LevelCamera/LevelCamera.hpp"
#include "../Import/Import.hpp"

#include <algorithm>
#include <cmath>
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
#include "Pine/Assets/Terrain/Terrain.hpp"
#include "Other/Actions/Actions.hpp"
#include "Other/TerrainSculpting/TerrainSculpting.hpp"
#include "Pine/Core/File/File.hpp"
#include "Pine/Engine/Engine.hpp"
#include "Pine/Core/Log/Log.hpp"
#include "Pine/Core/Serialization/Dump/SerializationDump.hpp"
#include "Pine/Core/String/String.hpp"
#include "Pine/Performance/Performance.hpp"
#include "Pine/Rendering/RenderManager/RenderManager.hpp"
#include "Pine/Rendering/Renderer3D/Specifications.hpp"
#include "Pine/World/Components/Light/Light.hpp"
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

    struct FloatParameter
    {
        bool Present = false;
        bool Valid = false;
        float Value = 0.f;
    };

    FloatParameter ReadFloatParameter(const Editor::DebugServer::Request& request, const std::string& name)
    {
        const auto parameter = request.Parameters.find(name);

        if (parameter == request.Parameters.end())
        {
            return {};
        }

        FloatParameter result;

        result.Present = true;

        try
        {
            std::size_t consumed = 0;

            result.Value = std::stof(parameter->second, &consumed);
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
        json["visibleTerrainChunks"] = statistics.VisibleTerrainChunkCount;
        json["culledTerrainChunks"] = statistics.CulledTerrainChunkCount;
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
            entry["shortName"] = scope->ShortName;
            entry["parent"] = scope->Parent != nullptr ? scope->Parent->Name : "";

            // Summed over the frame rather than per call, so a scope that runs once per rendering
            // context reports what the whole frame spent in it.
            entry["time"] = scope->Time;
            entry["smoothedTime"] = scope->SmoothedTime;
            entry["callCount"] = scope->CallCount;

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
        json["properties"] = Editor::DebugServer::Editing::ReadComponentProperties(component);

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
        body["properties"] = Editor::DebugServer::Editing::ReadEntityProperties(entity);

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

    // Resolves the ?path= / ?id= pair every asset endpoint accepts. Returns nullptr and fills
    // `error` with the reply to send, so the caller only has to forward it.
    Pine::Asset* ResolveRequestedAsset(const Editor::DebugServer::Request& request,
                                       Editor::DebugServer::Response& error)
    {
        const auto pathParameter = request.Parameters.find("path");
        const auto idParameter = request.Parameters.find("id");

        if (pathParameter != request.Parameters.end())
        {
            const auto asset = Pine::Assets::GetAssetByPath(pathParameter->second);

            if (asset == nullptr)
            {
                error = Editor::DebugServer::Error(404, fmt::format("No asset at path '{}'.", pathParameter->second));
            }

            return asset;
        }

        if (idParameter != request.Parameters.end())
        {
            const Pine::UId id{ std::string(idParameter->second) };

            if (!id.IsValid())
            {
                error = Editor::DebugServer::Error(400, fmt::format(
                    "'{}' is not a valid asset id. Ids look like '18d4ae6bff2fd8c6-213ebd6dcd8f0b73'.",
                    idParameter->second));

                return nullptr;
            }

            const auto asset = Pine::Assets::GetAssetByUId(id);

            if (asset == nullptr)
            {
                error = Editor::DebugServer::Error(404, fmt::format("No asset with id '{}'.", idParameter->second));
            }

            return asset;
        }

        error = Editor::DebugServer::Error(400, "Expected a ?path= or ?id= parameter. /assets lists both.");

        return nullptr;
    }

    Editor::DebugServer::Response GetAsset(const Editor::DebugServer::Request& request)
    {
        Editor::DebugServer::Response error;

        const auto asset = ResolveRequestedAsset(request, error);

        if (asset == nullptr)
        {
            return error;
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

    /* GET /terrain */

    nlohmann::json StoreGridCoordinate(const Pine::Vector2i coordinate)
    {
        nlohmann::json value;

        value["x"] = coordinate.x;
        value["z"] = coordinate.y;

        return value;
    }

    nlohmann::json StorePoint(const Pine::Vector3f point)
    {
        nlohmann::json value;

        value["x"] = point.x;
        value["y"] = point.y;
        value["z"] = point.z;

        return value;
    }

    // The lights occupying one chunk's slots, by the name of the entity each one is on, nearest
    // first - which is the order the scene processor fills the slots in.
    //
    // Split by kind because the two compete for separate slots: a chunk keeps the nearest five
    // point lights and the nearest two spot lights, and a flat list would make "the sixth lamp was
    // dropped" and "the third spot was dropped" look like the same answer. Empty slots are left
    // out rather than reported as null, so the length of each array is how many lights reach the
    // chunk at all.
    nlohmann::json StoreChunkLights(Pine::TerrainChunk& chunk)
    {
        namespace Slots = Pine::Renderer3D::Specifications::ObjectLightSlots;

        const auto storeRange = [&chunk](const int offset, const int count)
        {
            auto names = nlohmann::json::array();

            for (int i = 0; i < count; i++)
            {
                if (const auto light = chunk.LightSlots.Index[offset + i].Get())
                {
                    names.push_back(light->GetParent()->GetName());
                }
            }

            return names;
        };

        nlohmann::json lights;

        lights["point"] = storeRange(Slots::POINT_LIGHT_OFFSET, Slots::POINT_LIGHT_COUNT);
        lights["spot"] = storeRange(Slots::SPOT_LIGHT_OFFSET, Slots::SPOT_LIGHT_COUNT);

        return lights;
    }

    // Reports the terrain's layout and its chunk views, and optionally samples a height. The height
    // field itself is deliberately not included - it is tens of thousands of samples, and what a
    // caller actually wants to assert on is a height at a coordinate.
    Editor::DebugServer::Response GetTerrain(const Editor::DebugServer::Request& request)
    {
        Editor::DebugServer::Response error;

        const auto asset = ResolveRequestedAsset(request, error);

        if (asset == nullptr)
        {
            return error;
        }

        const auto terrain = dynamic_cast<Pine::Terrain*>(asset);

        if (terrain == nullptr)
        {
            return Editor::DebugServer::Error(400, fmt::format("Asset '{}' is a {}, not a Terrain.",
                asset->GetPath(), Pine::AssetTypeToString(asset->GetType())));
        }

        nlohmann::json layout;

        layout["chunkCount"] = StoreGridCoordinate(terrain->GetChunkCount());
        layout["chunkOrigin"] = StoreGridCoordinate(terrain->GetChunkOrigin());
        layout["chunkQuads"] = terrain->GetChunkQuads();
        layout["chunkSize"] = terrain->GetChunkSize();
        layout["sampleSpacing"] = terrain->GetSampleSpacing();
        layout["fieldSize"] = StoreGridCoordinate(terrain->GetFieldSize());
        layout["sampleMin"] = StoreGridCoordinate(terrain->GetSampleMin());
        layout["sampleMax"] = StoreGridCoordinate(terrain->GetSampleMax());
        layout["heightMin"] = terrain->GetHeightMin();
        layout["heightMax"] = terrain->GetHeightMax();
        layout["lodCount"] = terrain->GetLodCount();

        nlohmann::json chunks = nlohmann::json::array();

        for (auto& chunk : terrain->GetChunks())
        {
            nlohmann::json entry;

            entry["coordinate"] = StoreGridCoordinate(chunk.Coordinate);
            entry["boundsMin"] = StorePoint(chunk.BoundsMin);
            entry["boundsMax"] = StorePoint(chunk.BoundsMax);
            entry["dirty"] = chunk.IsDirty;
            entry["lights"] = StoreChunkLights(chunk);

            chunks.push_back(entry);
        }

        nlohmann::json body;

        body["path"] = terrain->GetPath();
        body["uid"] = terrain->GetUId().ToString();
        body["modified"] = terrain->HasBeenModified();
        body["layout"] = layout;
        body["chunkCount"] = chunks.size();
        body["chunks"] = chunks;

        // Every slot, filled or not, so that the index a caller reads here is the splat channel it
        // paints into. An empty slot is null rather than being left out.
        auto layers = nlohmann::json::array();

        for (int layer = 0; layer < Pine::Terrain::MaximumLayerCount; layer++)
        {
            const auto material = terrain->GetLayer(layer);

            layers.push_back(material != nullptr ? nlohmann::json(material->GetPath()) : nlohmann::json(nullptr));
        }

        body["layers"] = layers;
        body["splatMapReady"] = terrain->GetSplatMap() != nullptr;

        // ?x= and ?z= sample a terrain-local point, which is what the later units assert against:
        // where a dropped body should land, what a brush stroke moved.
        const auto x = ReadFloatParameter(request, "x");
        const auto z = ReadFloatParameter(request, "z");

        if (x.Present != z.Present)
        {
            return Editor::DebugServer::Error(400, "A height query needs both ?x= and ?z=.");
        }

        if (x.Present)
        {
            if (!x.Valid || !z.Valid)
            {
                return Editor::DebugServer::Error(400, "?x= and ?z= have to be numbers.");
            }

            nlohmann::json height;

            height["x"] = x.Value;
            height["z"] = z.Value;

            // Absent rather than clamped when the point is off the terrain - out of bounds is a
            // normal answer here, and a number would be a wrong one.
            const auto value = terrain->GetHeightAt(x.Value, z.Value);

            if (value.has_value())
            {
                height["y"] = *value;
            }
            else
            {
                height["y"] = nullptr;
            }

            body["height"] = height;

            // The layer weights at the nearest sample to the same point. Nearest rather than
            // interpolated because what a caller wants to assert on is what a brush stored, and
            // the interpolation between two stored samples is the shader's business.
            const auto spacing = terrain->GetSampleSpacing();

            const Pine::Vector2i sample = {
                static_cast<int>(std::lround(x.Value / spacing)),
                static_cast<int>(std::lround(z.Value / spacing))
            };

            if (const auto weights = terrain->GetSampleWeights(sample))
            {
                nlohmann::json entry;

                entry["sample"] = StoreGridCoordinate(sample);
                entry["weights"] = { weights->x, weights->y, weights->z, weights->w };

                body["layerWeights"] = entry;
            }
        }

        return { 200, body };
    }

    /* POST /terrain/sculpt */

    // One point of a stroke. Terrain-local, like every coordinate the terrain endpoints deal in.
    struct SculptPoint
    {
        float X = 0.f;
        float Z = 0.f;
    };

    // Reads a required finite number out of a JSON object, or leaves `error` set. Separate from the
    // query-parameter reader above because a sculpt request arrives as a body: a stroke carries a
    // list of points, which a query string has no good way to express.
    bool ReadNumber(const nlohmann::json& body,
                    const char* name,
                    float& value,
                    Editor::DebugServer::Response& error)
    {
        if (!body.contains(name))
        {
            return true;
        }

        if (!body[name].is_number())
        {
            error = Editor::DebugServer::Error(400, fmt::format("\"{}\" has to be a number.", name));

            return false;
        }

        const auto read = body[name].get<float>();

        if (!std::isfinite(read))
        {
            error = Editor::DebugServer::Error(400, fmt::format("\"{}\" has to be a finite number.", name));

            return false;
        }

        value = read;

        return true;
    }

    // How many points one request may carry. A stroke is applied synchronously between frames, so
    // this is what stops a single request from stalling the editor for an unbounded time.
    constexpr std::size_t MaximumSculptPoints = 256;

    // Applies a brush stroke to a terrain, as one undo step. The stroke either moves the ground or
    // paints a layer onto it, which is what its mode says.
    //
    // The whole stroke arrives in one request rather than one request per point, because a stroke
    // is the unit of undo: splitting it across requests would either record one step per point or
    // need the server to hold a stroke open between them, and a client that died mid-drag would
    // leave it open forever.
    Editor::DebugServer::Response PostTerrainSculpt(const Editor::DebugServer::Request& request)
    {
        // Sculpting writes to an asset, and undo is refused while playing - so a stroke applied now
        // would be one the author could not take back. PlayHandler also restores a world snapshot
        // on Stop(), and a terrain edited underneath it is not part of that snapshot.
        if (PlayHandler::GetGameState() != PlayHandler::EditorGameState::Stopped)
        {
            return Editor::DebugServer::Error(409, fmt::format(
                "Cannot sculpt while the editor is {}. Stop play mode first.",
                GameStateToString(PlayHandler::GetGameState())));
        }

        Editor::DebugServer::Response error;

        const auto asset = ResolveRequestedAsset(request, error);

        if (asset == nullptr)
        {
            return error;
        }

        const auto terrain = dynamic_cast<Pine::Terrain*>(asset);

        if (terrain == nullptr)
        {
            return Editor::DebugServer::Error(400, fmt::format("Asset '{}' is a {}, not a Terrain.",
                asset->GetPath(), Pine::AssetTypeToString(asset->GetType())));
        }

        const auto body = request.Body.empty() ? nlohmann::json::object()
                                               : nlohmann::json::parse(request.Body, nullptr, false);

        if (body.is_discarded() || !body.is_object())
        {
            return Editor::DebugServer::Error(400, "Request body has to be a JSON object.");
        }

        Editor::TerrainSculpting::Brush brush;

        if (body.contains("mode"))
        {
            if (!body["mode"].is_string())
            {
                return Editor::DebugServer::Error(400, "\"mode\" has to be a string.");
            }

            const auto mode = Editor::TerrainSculpting::BrushModeFromString(body["mode"].get<std::string>());

            if (!mode.has_value())
            {
                return Editor::DebugServer::Error(400, fmt::format(
                    "'{}' is not a brush mode. Use raise, lower, smooth, flatten or paint.",
                    body["mode"].get<std::string>()));
            }

            brush.Mode = *mode;
        }

        // Which splat channel a paint stroke writes into. Read whatever the mode is, so that a
        // request naming a layer the mode ignores is rejected rather than silently accepted.
        if (body.contains("layer"))
        {
            if (!body["layer"].is_number_integer())
            {
                return Editor::DebugServer::Error(400, "\"layer\" has to be a whole number.");
            }

            brush.Layer = body["layer"].get<int>();

            if (brush.Layer < 0 || brush.Layer >= Pine::Terrain::MaximumLayerCount)
            {
                return Editor::DebugServer::Error(400, fmt::format(
                    "\"layer\" has to be between 0 and {}; this one is {}.",
                    Pine::Terrain::MaximumLayerCount - 1, brush.Layer));
            }
        }

        // Brush time in seconds, standing in for the frame time a dragged stroke would accumulate.
        // Every point of the stroke gets this much, so a five-point stroke moves the ground five
        // times as far - exactly as holding the brush still for five frames would.
        float duration = 0.1f;

        if (!ReadNumber(body, "radius", brush.Radius, error) ||
            !ReadNumber(body, "strength", brush.Strength, error) ||
            !ReadNumber(body, "falloff", brush.Falloff, error) ||
            !ReadNumber(body, "duration", duration, error))
        {
            return error;
        }

        if (brush.Radius <= 0.f)
        {
            return Editor::DebugServer::Error(400, "\"radius\" has to be above zero.");
        }

        if (duration <= 0.f)
        {
            return Editor::DebugServer::Error(400, "\"duration\" has to be above zero.");
        }

        if (body.contains("height"))
        {
            float flattenHeight = 0.f;

            if (!ReadNumber(body, "height", flattenHeight, error))
            {
                return error;
            }

            brush.FlattenHeight = flattenHeight;
        }

        // Either one point, which is a dab, or a list of them, which is a drag. Both are one stroke
        // and so one undo step.
        std::vector<SculptPoint> points;

        if (body.contains("points"))
        {
            if (!body["points"].is_array() || body["points"].empty())
            {
                return Editor::DebugServer::Error(400, "\"points\" has to be a non-empty array.");
            }

            if (body["points"].size() > MaximumSculptPoints)
            {
                return Editor::DebugServer::Error(400, fmt::format(
                    "A stroke carries at most {} points; this one has {}.",
                    MaximumSculptPoints, body["points"].size()));
            }

            for (const auto& entry : body["points"])
            {
                if (!entry.is_object())
                {
                    return Editor::DebugServer::Error(400, "Every entry of \"points\" has to be an object {\"x\", \"z\"}.");
                }

                SculptPoint point;

                if (!ReadNumber(entry, "x", point.X, error) || !ReadNumber(entry, "z", point.Z, error))
                {
                    return error;
                }

                points.push_back(point);
            }
        }
        else
        {
            SculptPoint point;

            if (!ReadNumber(body, "x", point.X, error) || !ReadNumber(body, "z", point.Z, error))
            {
                return error;
            }

            if (!body.contains("x") || !body.contains("z"))
            {
                return Editor::DebugServer::Error(400,
                    "Expected a point as \"x\" and \"z\", or a stroke as \"points\": [{\"x\", \"z\"}].");
            }

            points.push_back(point);
        }

        // Any UI edit still being held has to close before this stroke is written, or the two land
        // in the history in the wrong order. RegisterCommand does this too, but a stroke that moves
        // nothing never gets that far.
        Editor::Actions::FinishHeldCommand();

        std::size_t applied = 0;

        for (const auto& point : points)
        {
            if (Editor::TerrainSculpting::Apply(terrain, brush, { point.X, point.Z }, duration))
            {
                applied++;
            }
        }

        // Ends the stroke whether or not anything was applied: a stroke entirely off the terrain
        // leaves nothing open, and one that did move ground is recorded here as a single step.
        Editor::TerrainSculpting::EndStroke();

        nlohmann::json result;

        result["terrain"] = terrain->GetPath();
        result["mode"] = Editor::TerrainSculpting::BrushModeToString(brush.Mode);

        if (brush.Mode == Editor::TerrainSculpting::BrushMode::Paint)
        {
            result["layer"] = brush.Layer;
        }

        // How many of the points actually reached the terrain. Fewer than were sent is a normal
        // answer for a stroke dragged over the edge, and zero is how a caller learns that the
        // coordinates it is using are not on this terrain at all.
        result["appliedPoints"] = applied;
        result["requestedPoints"] = points.size();

        const auto state = Editor::Actions::GetHistoryState();

        nlohmann::json history;

        history["undoCount"] = state.UndoCount;
        history["redoCount"] = state.RedoCount;

        result["history"] = history;

        return { 200, result };
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
    AddRoute(Method::Get, "/logs", LogHistory::Get);
    AddRoute(Method::Get, "/entities", GetEntities);
    AddRoute(Method::Get, "/entity", GetEntity);
    AddRoute(Method::Post, "/spatial/query", Spatial::Query);
    AddRoute(Method::Get, "/stats", GetStats);
    AddRoute(Method::Get, "/assets", GetAssets);
    AddMutationRoute("/assets/import", Import::Execute);
    AddRoute(Method::Get, "/asset", GetAsset);
    AddRoute(Method::Get, "/terrain", GetTerrain);
    AddMutationRoute("/terrain/sculpt", PostTerrainSculpt);
    AddRoute(Method::Get, "/viewport.png", GetViewportPng);

    AddRoute(Method::Post, "/observe", Observation::Begin);
    AddRoute(Method::Post, "/pick", Picking::Pick);
    AddMutationRoute("/level/load", PostLevelLoad);
    AddRoute(Method::Get, "/level/status", Persistence::Get);
    AddMutationRoute("/level/save", Persistence::Save);
    AddMutationRoute("/level/save-as", Persistence::SaveAs);
    AddRoute(Method::Get, "/level/camera", LevelCamera::Get);
    AddMutationRoute("/level/camera", LevelCamera::Set);
    AddRoute(Method::Get, "/edit/schema", Editing::GetSchema);
    AddMutationRoute("/edit", Editing::Edit);
    AddRoute(Method::Get, "/history", Editing::History::Get);
    AddMutationRoute("/history/undo", Editing::History::Undo);
    AddMutationRoute("/history/redo", Editing::History::Redo);
    AddRoute(Method::Get, "/camera", Camera::Get);
    AddMutationRoute("/camera", Camera::Set);
    AddMutationRoute("/camera/frame", Camera::Frame);
}

Editor::DebugServer::Response Editor::DebugServer::Endpoints::ReadEntity(const Request& request)
{
    return GetEntity(request);
}
