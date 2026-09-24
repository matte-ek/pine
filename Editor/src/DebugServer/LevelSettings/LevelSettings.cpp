#include "LevelSettings.hpp"

#include "../Editing/History/History.hpp"
#include "../Editing/Values/Values.hpp"
#include "Other/Actions/Actions.hpp"
#include "Other/PlayHandler/PlayHandler.hpp"
#include "Pine/Assets/Level/Level.hpp"
#include "Pine/Assets/Texture3D/Texture3D.hpp"
#include "Pine/Core/Serialization/Json/SerializationJson.hpp"
#include "Pine/World/World.hpp"

namespace
{
    using nlohmann::json;
    using namespace Editor::DebugServer;
    namespace Values = Editing::Values;

    // The advertised range keys are the ones Values actually enforces. `uiRange` is the span the
    // Level Properties panel offers and is guidance only - its sliders deliberately let a value be
    // typed outside them, so the API does not impose a ceiling the editor does not have.
    const json& PropertySchema()
    {
        static const json schema = {
            { "Skybox", { { "type", "asset" }, { "assetType", "Texture3D" }, { "nullable", true } } },
            { "AmbientColor", { { "type", "vector3" }, { "minimum", 0 }, { "colorSpace", "linear" },
                { "uiRange", { 0, 1 } } } },
            { "FogColor", { { "type", "vector4" }, { "minimum", 0 }, { "colorSpace", "linear" },
                { "uiRange", { 0, 1 } } } },
            { "FogDistance", { { "type", "number" }, { "minimum", 0.01f }, { "units", "world units" },
                { "uiRange", { 1, 250 } } } },
            { "FogIntensity", { { "type", "number" }, { "minimum", 0 }, { "uiRange", { 0, 1 } } } },
            { "Exposure", { { "type", "number" }, { "minimum", 0 }, { "uiRange", { 0, 8 } } } },
            { "BloomThreshold", { { "type", "number" }, { "minimum", 0 }, { "uiRange", { 0, 5 } } } },
            { "BloomIntensity", { { "type", "number" }, { "minimum", 0 }, { "uiRange", { 0, 2 } } } },
            { "GrainStrength", { { "type", "number" }, { "minimum", 0 }, { "uiRange", { 0, 0.3f } } } },
            { "VignetteStrength", { { "type", "number" }, { "minimum", 0 }, { "uiRange", { 0, 1 } } } },
            { "WindDirection", { { "type", "number" }, { "units", "degrees" }, { "uiRange", { 0, 360 } } } },
            { "WindStrength", { { "type", "number" }, { "minimum", 0 }, { "uiRange", { 0, 1 } } } },
            { "WindSpeed", { { "type", "number" }, { "minimum", 0 }, { "units", "gusts per second" },
                { "uiRange", { 0, 3 } } } }
        };
        return schema;
    }

    json LevelIdentity(const Pine::Level* level)
    {
        if (level == nullptr)
        {
            return nullptr;
        }
        return { { "path", level->GetPath() }, { "id", level->GetUId().ToString() } };
    }
}

json Editor::DebugServer::LevelSettings::Read(const Pine::LevelSettings& settings)
{
    return {
        { "Skybox", Values::AssetReference(settings.Skybox.Get()) },
        { "AmbientColor", Pine::SerializationJson::StoreVector3(settings.AmbientColor) },
        { "FogColor", Pine::SerializationJson::StoreVector4(settings.FogColor) },
        { "FogDistance", settings.FogDistance },
        { "FogIntensity", settings.FogIntensity },
        { "Exposure", settings.Exposure },
        { "BloomThreshold", settings.BloomThreshold },
        { "BloomIntensity", settings.BloomIntensity },
        { "GrainStrength", settings.GrainStrength },
        { "VignetteStrength", settings.VignetteStrength },
        { "WindDirection", settings.WindDirection },
        { "WindStrength", settings.WindStrength },
        { "WindSpeed", settings.WindSpeed }
    };
}

void Editor::DebugServer::LevelSettings::Apply(Pine::LevelSettings& settings, const json& state)
{
    settings.Skybox = dynamic_cast<Pine::Texture3D*>(Values::ResolvedAsset(state.at("Skybox")));
    settings.AmbientColor = Values::Vector3(state.at("AmbientColor"));
    settings.FogColor = Values::Vector4(state.at("FogColor"));
    settings.FogDistance = state.at("FogDistance").get<float>();
    settings.FogIntensity = state.at("FogIntensity").get<float>();
    settings.Exposure = state.at("Exposure").get<float>();
    settings.BloomThreshold = state.at("BloomThreshold").get<float>();
    settings.BloomIntensity = state.at("BloomIntensity").get<float>();
    settings.GrainStrength = state.at("GrainStrength").get<float>();
    settings.VignetteStrength = state.at("VignetteStrength").get<float>();
    settings.WindDirection = state.at("WindDirection").get<float>();
    settings.WindStrength = state.at("WindStrength").get<float>();
    settings.WindSpeed = state.at("WindSpeed").get<float>();
}

Editor::DebugServer::Response Editor::DebugServer::LevelSettings::Get(const Request&)
{
    const auto level = Pine::World::GetActiveLevel();
    if (level == nullptr)
    {
        return Error(409, "No level is active.");
    }

    return { 200, { { "level", LevelIdentity(level) }, { "properties", Read(level->GetLevelSettings()) } } };
}

Editor::DebugServer::Response Editor::DebugServer::LevelSettings::Set(const Request& request)
{
    if (PlayHandler::GetGameState() != PlayHandler::EditorGameState::Stopped)
    {
        return Error(409, "Stop play mode before editing level settings.");
    }

    const auto level = Pine::World::GetActiveLevel();
    if (level == nullptr)
    {
        return Error(409, "No level is active.");
    }

    json state;
    Editing::History::Snapshot before;
    try
    {
        Values::Require(request.Body.size() <= 4096, "", "Request exceeds 4 KiB.");
        const auto depthLimit = [](int depth, json::parse_event_t, json&)
        {
            Values::Require(depth <= 8, "", "JSON nesting exceeds 8 levels.");
            return true;
        };
        const auto body = json::parse(request.Body, depthLimit, false);
        Values::Object(body, "", { "properties" }, { "properties" });

        const auto& properties = body.at("properties");
        Values::Require(properties.is_object(), "/properties", "Expected a property object.");
        Values::Require(!properties.empty(), "/properties", "Expected at least one property to change.");

        // Merge over the live values, so a request names only what it changes. Merging is per
        // property: a supplied colour replaces the whole colour rather than one channel.
        state = Read(level->GetLevelSettings());
        state.update(properties);
        Values::Properties(state, PropertySchema(), "/properties");

        Editor::Actions::FinishHeldCommand();
        before = Editing::History::Capture();
    }
    catch (const Values::ValidationError& exception)
    {
        return { 400, { { "error", exception.what() }, { "path", exception.Path }, { "phase", "validation" } } };
    }
    catch (const std::exception& exception)
    {
        return Error(400, exception.what());
    }

    Apply(level->GetLevelSettings(), state);

    try
    {
        Editing::History::Record(std::move(before), Editing::History::Capture());
        auto response = Get(request);
        response.Body["history"] = "recorded";
        return response;
    }
    catch (const std::exception& exception)
    {
        Editor::Actions::ClearHistory();
        return { 500, { { "error", exception.what() }, { "history", "cleared" },
            { "stateMayHaveChanged", true }, { "failedOperationMayHaveChangedState", true } } };
    }
}

nlohmann::json Editor::DebugServer::LevelSettings::Schema()
{
    return {
        { "get", "/level/settings" }, { "set", "/level/settings" }, { "method", "POST" },
        { "requiredPlayState", "Stopped" }, { "undo", true },
        { "request", {
            { "type", "object" }, { "required", { "properties" } }, { "additionalFields", false },
            { "fields", { { "properties", {
                { "type", "object" }, { "partial", true },
                { "description", "Atmosphere and post-processing settings of the active Level. Omitted properties keep their current value. The Game camera is chosen through /level/camera instead." }
            } } } }
        } },
        { "properties", PropertySchema() }
    };
}
