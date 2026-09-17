#include "Observation.hpp"

#include <algorithm>
#include <cmath>
#include <optional>

#include "../Editing/Values/Values.hpp"
#include "../Endpoints/Endpoints.hpp"
#include "../LogHistory/LogHistory.hpp"
#include "../Screenshot/Screenshot.hpp"
#include "../Requests/Requests.hpp"
#include "../Picking/Picking.hpp"
#include "Other/PlayHandler/PlayHandler.hpp"
#include "Rendering/RenderHandler.hpp"
#include "Pine/Core/Log/Log.hpp"
#include "Pine/Core/Serialization/Json/SerializationJson.hpp"
#include "Pine/World/Entities/Entities.hpp"

namespace
{
    using nlohmann::json;
    using namespace Editor::DebugServer;
    namespace Values = Editing::Values;

    std::uint64_t m_Frame = 0;
    std::uint64_t m_Revision = 0;
    std::uint64_t m_RenderedRevision = 0;
    std::uint64_t m_RenderedGeneration = 0;

    struct RenderedView
    {
        Pine::RenderingContext Context;
        json Camera;
        Pine::Matrix4f ViewProjection;
    };

    std::optional<RenderedView> m_LevelView;
    std::optional<RenderedView> m_GameView;

    struct Options
    {
        std::string View = "level";
        int Width = 0;
        bool Picking = false;
        json After;
        std::uint64_t AcceptedFrame = 0;
        std::uint64_t LogsSince = 0;
        std::vector<std::string> Entities;
    };

    std::uint64_t LogCursor()
    {
        const auto messages = Pine::Log::GetLogSnapshot();
        return messages.empty() ? 0 : messages.back().Sequence;
    }

    json Token(std::uint64_t logCursor)
    {
        return {
            { "session", Requests::GetSession() }, { "sceneGeneration", Pine::Entities::GetSceneGeneration() },
            { "revision", m_Revision }, { "frame", m_Frame }, { "logsSince", logCursor }
        };
    }

    std::uint64_t ReadCounter(const json& value, const std::string& path)
    {
        Values::Require(value.is_number_unsigned() || (value.is_number_integer() && value.get<std::int64_t>() >= 0),
            path, "Expected a nonnegative integer.");
        return value.get<std::uint64_t>();
    }

    json StoreMatrix(const Pine::Matrix4f& matrix)
    {
        auto columns = json::array();
        for (int column = 0; column < 4; ++column)
        {
            columns.push_back({ matrix[column][0], matrix[column][1], matrix[column][2], matrix[column][3] });
        }
        return columns;
    }

    json DescribeCamera(const Pine::Camera* camera)
    {
        const auto transform = camera->GetTransform();
        return {
            { "id", camera->GetId().ToString() },
            { "position", Pine::SerializationJson::StoreVector3(transform->GetPosition()) },
            { "rotation", Pine::SerializationJson::StoreQuaternion(transform->GetRotation()) },
            { "fieldOfView", camera->GetFieldOfView() },
            { "nearPlane", camera->GetNearPlane() }, { "farPlane", camera->GetFarPlane() },
            { "viewMatrix", StoreMatrix(camera->GetViewMatrix()) },
            { "projectionMatrix", StoreMatrix(camera->GetProjectionMatrix()) }
        };
    }

    std::string EncodeBase64(const std::vector<std::uint8_t>& bytes)
    {
        constexpr const char* alphabet = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
        std::string result;
        result.reserve((bytes.size() + 2) / 3 * 4);
        for (std::size_t index = 0; index < bytes.size(); index += 3)
        {
            const bool hasSecond = index + 1 < bytes.size();
            const bool hasThird = index + 2 < bytes.size();
            const std::uint32_t bits = (static_cast<std::uint32_t>(bytes[index]) << 16)
                | (hasSecond ? static_cast<std::uint32_t>(bytes[index + 1]) << 8 : 0)
                | (hasThird ? bytes[index + 2] : 0);
            result.push_back(alphabet[(bits >> 18) & 63]);
            result.push_back(alphabet[(bits >> 12) & 63]);
            result.push_back(hasSecond ? alphabet[(bits >> 6) & 63] : '=');
            result.push_back(hasThird ? alphabet[bits & 63] : '=');
        }
        return result;
    }

    Response Complete(const Options& options)
    {
        if (Pine::Entities::GetSceneGeneration() != options.After.at("sceneGeneration").get<std::uint64_t>()
            || m_RenderedGeneration != Pine::Entities::GetSceneGeneration())
        {
            return Error(409, "Scene was replaced before the observation could be captured. Request a new observation.");
        }
        if (m_Frame <= options.AcceptedFrame || m_RenderedRevision < options.After.at("revision").get<std::uint64_t>())
        {
            return Error(409, "The requested operation has not reached a rendered frame.");
        }

        const auto& view = options.View == "level" ? m_LevelView : m_GameView;
        if (!view)
        {
            return Error(409, "The requested viewport did not render a 3D perspective view. Open its tab and ensure it has a camera.");
        }

        // These reads happen after rendering, before the UI and before queued edits.
        // Keep them separate from the camera snapshot, which was taken at RenderContext.
        auto entities = json::array();
        for (const auto& id : options.Entities)
        {
            Request request;
            request.Parameters["id"] = id;
            auto entity = Endpoints::ReadEntity(request);
            if (entity.StatusCode != 200)
            {
                return Error(409, "An observed entity no longer exists: " + id);
            }
            entities.push_back(std::move(entity.Body));
        }

        std::vector<std::uint8_t> png;
        if (!Screenshot::CapturePng(&view->Context, options.Width, png))
        {
            return Error(409, "The rendered viewport could not be captured.");
        }

        Request logRequest;
        logRequest.Parameters["since"] = std::to_string(options.LogsSince);
        auto logs = LogHistory::Get(logRequest);
        if (logs.StatusCode != 200)
        {
            return logs;
        }

        const auto sourceWidth = static_cast<int>(view->Context.Size.x);
        const auto sourceHeight = static_cast<int>(view->Context.Size.y);
        const auto width = options.Width > 0 ? std::min(options.Width, sourceWidth) : sourceWidth;
        const auto height = std::max(1, static_cast<int>(std::lround(static_cast<double>(sourceHeight) * width / sourceWidth)));

        const json frame = {
            { "session", Requests::GetSession() }, { "id", m_Frame },
            { "sceneGeneration", m_RenderedGeneration }, { "revision", m_RenderedRevision }
        };
        json picking = nullptr;
        if (options.Picking)
        {
            auto captured = Picking::Capture(view->Context, view->ViewProjection, width, height, frame);
            if (captured.StatusCode != 200)
            {
                return captured;
            }
            picking = std::move(captured.Body);
        }

        return { 200, {
            { "after", options.After },
            { "frame", frame },
            { "picking", picking },
            { "viewport", { { "view", options.View }, { "width", sourceWidth }, { "height", sourceHeight } } },
            { "camera", view->Camera },
            { "image", {
                { "contentType", "image/png" }, { "encoding", "base64" },
                { "width", width }, { "height", height }, { "data", EncodeBase64(png) }
            } },
            { "entities", entities }, { "logs", logs.Body },
            { "timing", {
                { "camera", "render-context" }, { "image", "post-render-before-ui" },
                { "entities", "post-render-before-ui" }, { "logs", "after-capture-before-ui" }
            } }
        } };
    }
}

void Editor::DebugServer::Observation::OnRender(Pine::RenderingContext* context, Pine::RenderStage stage)
{
    if (stage == Pine::RenderStage::PreRender)
    {
        ++m_Frame;
        m_RenderedRevision = m_Revision;
        m_RenderedGeneration = Pine::Entities::GetSceneGeneration();
        m_LevelView.reset();
        m_GameView.reset();
        return;
    }
    if (stage != Pine::RenderStage::RenderContext || context == nullptr)
    {
        return;
    }

    // RenderContext only fires for active contexts. Snapshot size and matrices now:
    // a later context updates camera matrices again, and ImGui may resize the panel.
    if (!context->UseRenderPipeline || context->SceneCamera == nullptr || context->FrameBuffer == nullptr
        || context->SceneCamera->GetCameraType() != Pine::CameraType::Perspective)
    {
        return;
    }
    const auto bufferSize = context->FrameBuffer->GetSize();
    if (context->Size.x < 1 || context->Size.y < 1
        || context->Size.x > bufferSize.x || context->Size.y > bufferSize.y)
    {
        return;
    }

    if (context == Editor::RenderHandler::GetLevelRenderingContext())
    {
        m_LevelView = RenderedView{ *context, DescribeCamera(context->SceneCamera),
            context->SceneCamera->GetProjectionMatrix() * context->SceneCamera->GetViewMatrix() };
    }
    else if (context == Editor::RenderHandler::GetGameRenderingContext())
    {
        m_GameView = RenderedView{ *context, DescribeCamera(context->SceneCamera),
            context->SceneCamera->GetProjectionMatrix() * context->SceneCamera->GetViewMatrix() };
    }
}

Editor::DebugServer::Handler Editor::DebugServer::Observation::TrackMutation(Handler handler)
{
    return [handler = std::move(handler)](const Request& request)
    {
        const auto logsSince = LogCursor();
        auto response = handler(request);
        // Editing reports completed operations on execution failure. Such partial writes
        // also need an observation token; rejected validation does not advance revision.
        const bool partialWrite = response.Body.is_object() && response.Body.value("failedOperationMayHaveChangedState", false);
        if ((response.StatusCode >= 200 && response.StatusCode < 300) || partialWrite)
        {
            ++m_Revision;
            response.Body["observationToken"] = Token(logsSince);
        }
        return response;
    };
}

Editor::DebugServer::Response Editor::DebugServer::Observation::Begin(const Request& request)
{
    try
    {
        Values::Require(request.Body.size() <= 16 * 1024, "", "Observation request exceeds 16 KiB.");
        const auto depthLimit = [](int depth, json::parse_event_t, json&)
        {
            Values::Require(depth <= 8, "", "Observation JSON nesting exceeds 8 levels.");
            return true;
        };
        const auto body = json::parse(request.Body, depthLimit, false);
        Values::Require(!body.is_discarded(), "", "Request body is not valid JSON.");
        Values::Object(body, "", { "after", "view", "width", "entities", "logsSince", "picking" });

        Options options;
        if (body.contains("picking"))
        {
            Values::Require(body.at("picking").is_boolean(), "/picking", "Expected a boolean.");
            options.Picking = body.at("picking").get<bool>();
            if (options.Picking && PlayHandler::GetGameState() != PlayHandler::EditorGameState::Stopped)
            {
                return Error(409, "Picking captures require stopped edit mode.");
            }
        }
        options.AcceptedFrame = m_Frame;
        options.After = body.value("after", Token(LogCursor()));
        Values::Object(options.After, "/after", { "session", "sceneGeneration", "revision", "frame", "logsSince" },
            { "session", "sceneGeneration", "revision", "frame", "logsSince" });
        const auto session = Values::String(options.After.at("session"), "/after/session");
        const auto generation = ReadCounter(options.After.at("sceneGeneration"), "/after/sceneGeneration");
        const auto revision = ReadCounter(options.After.at("revision"), "/after/revision");
        const auto frame = ReadCounter(options.After.at("frame"), "/after/frame");
        options.LogsSince = ReadCounter(options.After.at("logsSince"), "/after/logsSince");
        if (session != Requests::GetSession() || generation != Pine::Entities::GetSceneGeneration())
        {
            return Error(409, "Observation token belongs to a different server session or scene generation.");
        }
        Values::Require(revision <= m_Revision && frame <= m_Frame, "/after", "Observation token refers to a future operation.");

        if (body.contains("view"))
        {
            options.View = Values::String(body.at("view"), "/view");
            Values::Require(options.View == "level" || options.View == "game", "/view", "Expected level or game.");
        }
        if (body.contains("width"))
        {
            const auto width = ReadCounter(body.at("width"), "/width");
            Values::Require(width >= 1 && width <= 4096, "/width", "Width must be between 1 and 4096.");
            options.Width = static_cast<int>(width);
        }
        if (body.contains("logsSince"))
        {
            options.LogsSince = ReadCounter(body.at("logsSince"), "/logsSince");
        }
        Values::Require(options.LogsSince <= LogCursor(), "/logsSince", "Log cursor is ahead of the current history.");
        if (body.contains("entities"))
        {
            const auto& entities = body.at("entities");
            Values::Require(entities.is_array() && entities.size() <= 128, "/entities", "Expected up to 128 entity IDs.");
            for (std::size_t index = 0; index < entities.size(); ++index)
            {
                const auto path = "/entities/" + std::to_string(index);
                const auto id = Values::Id(entities[index], path);
                const auto entity = Pine::Entities::Find(id);
                Values::Require(entity != nullptr && !entity->GetTemporary(), path, "Expected an existing scene entity.");
                options.Entities.push_back(id.ToString());
            }
        }

        Response response;
        response.Resume = [options] { return Complete(options); };
        return response;
    }
    catch (const Values::ValidationError& exception)
    {
        return { 400, { { "error", exception.what() }, { "path", exception.Path } } };
    }
}
