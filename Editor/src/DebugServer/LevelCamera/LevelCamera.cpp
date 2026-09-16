#include "LevelCamera.hpp"

#include "../Editing/Editing.hpp"
#include "../Editing/Components/Components.hpp"
#include "../Editing/History/History.hpp"
#include "../Editing/Values/Values.hpp"
#include "Other/Actions/Actions.hpp"
#include "Other/PlayHandler/PlayHandler.hpp"
#include "Pine/Rendering/RenderManager/RenderManager.hpp"
#include "Pine/World/Entities/Entities.hpp"

namespace
{
    using nlohmann::json;
    using namespace Editor::DebugServer;
    namespace Values = Editing::Values;
}

Editor::DebugServer::Response Editor::DebugServer::LevelCamera::Get(const Request&)
{
    const auto context = Pine::RenderManager::GetPrimaryRenderingContext();
    const auto camera = context == nullptr ? nullptr : context->SceneCamera;
    json target = nullptr;
    json component = nullptr;
    if (camera != nullptr)
    {
        target = { { "id", camera->GetParent()->GetId().ToString() } };
        component = { { "id", camera->GetId().ToString() } };
    }
    return { 200, { { "target", target }, { "component", component },
        { "sceneGeneration", Pine::Entities::GetSceneGeneration() } } };
}

Editor::DebugServer::Response Editor::DebugServer::LevelCamera::Set(const Request& request)
{
    if (PlayHandler::GetGameState() != PlayHandler::EditorGameState::Stopped)
    {
        return Error(409, "Stop play mode before choosing the game camera.");
    }
    const auto context = Pine::RenderManager::GetPrimaryRenderingContext();
    if (context == nullptr)
    {
        return Error(409, "No game rendering context is available.");
    }

    Pine::Camera* camera = nullptr;
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
        Values::Object(body, "", { "target" }, { "target" });
        const auto& target = body.at("target");
        if (!target.is_null())
        {
            Values::Object(target, "/target", { "id" }, { "id" });
            const auto entity = Pine::Entities::Find(Values::Id(target.at("id"), "/target/id"));
            Values::Require(entity != nullptr && !Editing::ReadEntityProperties(entity).is_null(),
                "/target/id", "Expected an editable entity in the current scene.");
            camera = entity->GetComponent<Pine::Camera>();
            Values::Require(camera != nullptr, "/target/id", "Entity has no Camera component.");
            const auto adapter = Editing::Components::Find(Pine::ComponentType::Camera);
            Editing::Components::Prepare(*adapter, adapter->Read(nullptr), adapter->Read(camera), "/target/properties");
        }
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

    context->SceneCamera = camera;
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

nlohmann::json Editor::DebugServer::LevelCamera::Schema()
{
    return {
        { "get", "/level/camera" }, { "set", "/level/camera" }, { "method", "POST" },
        { "requiredPlayState", "Stopped" }, { "undo", true },
        { "request", {
            { "type", "object" }, { "required", { "target" } }, { "additionalFields", false },
            { "fields", { { "target", {
                { "type", "reference" }, { "kind", "entity" }, { "forms", { "id" } },
                { "nullable", true }, { "whenNull", "clearGameCamera" },
                { "description", "Selects the entity's first Camera, matching the editor and Level serialization. Requires valid perspective properties." }
            } } } }
        } }
    };
}
