#include "Capture.hpp"

#include <algorithm>
#include <cmath>
#include <deque>
#include <string>
#include <vector>

#include "../Catalog/Catalog.hpp"
#include "../Editing/Values/Values.hpp"
#include "../Observation/Observation.hpp"
#include "../Screenshot/Screenshot.hpp"

#include "Rendering/AssetPreview/AssetPreview.hpp"
#include "Rendering/RenderHandler.hpp"

#include "Pine/Assets/Asset/Asset.hpp"
#include "Pine/Core/Log/Log.hpp"
#include "Pine/Core/Serialization/Json/SerializationJson.hpp"
#include "Pine/Graphics/Graphics.hpp"
#include "Pine/Graphics/Interfaces/IFrameBuffer.hpp"
#include "Pine/World/Components/Camera/Camera.hpp"
#include "Pine/World/Components/Transform/Transform.hpp"
#include "Pine/World/Entities/Entities.hpp"
#include "Pine/World/Entity/Entity.hpp"

namespace
{
    using nlohmann::json;
    using namespace Editor::DebugServer;
    namespace Values = Editing::Values;

    constexpr int m_MinimumSize = 16;
    constexpr int m_MaximumSize = 4096;

    constexpr int m_DefaultPreviewSize = 512;
    constexpr int m_DefaultSceneWidth = 1280;
    constexpr int m_DefaultSceneHeight = 720;

    // Fixed rather than inherited from the editor camera, so the same request gives the same image
    // whatever the user happens to have the Level viewport set to. The far plane is far longer than
    // Camera's own default of 150, which clips most of a level-sized scene.
    constexpr float m_DefaultFieldOfView = 70.f;
    constexpr float m_DefaultNearPlane = 0.1f;
    constexpr float m_DefaultFarPlane = 1000.f;

    // One capture renders per frame, so this is how far behind a request may queue before the
    // five-second deadline makes admitting it pointless.
    constexpr std::size_t m_MaximumQueuedCaptures = 16;

    Pine::Graphics::IFrameBuffer* m_PreviewBuffer = nullptr;
    Pine::Graphics::IFrameBuffer* m_SceneBuffer = nullptr;

    // What one queued /render request asked for. Held rather than applied immediately because the
    // camera has to be moved between frames, not in the middle of one.
    struct Request3D
    {
        std::uint64_t Ticket = 0;

        Pine::Vector3f Position = Pine::Vector3f(0.f);
        Pine::Quaternion Rotation = Pine::Quaternion(1.f, 0.f, 0.f, 0.f);

        float FieldOfView = m_DefaultFieldOfView;
        float NearPlane = m_DefaultNearPlane;
        float FarPlane = m_DefaultFarPlane;

        Pine::Vector2i Size = Pine::Vector2i(m_DefaultSceneWidth, m_DefaultSceneHeight);

        std::uint64_t SceneGeneration = 0;
    };

    // The debug server's own rendering context, alongside the editor's Level and Game ones. It is
    // inactive except for the single frame a queued capture is armed for, so an editor that never
    // captures renders exactly what it did before.
    Pine::RenderingContext m_Context;
    Pine::Entity* m_CameraEntity = nullptr;

    std::deque<Request3D> m_Queue;
    std::uint64_t m_NextTicket = 1;

    // The ticket the frame currently being rendered belongs to, or zero. Set at PreRender and
    // claimed at PostRender by that request's own continuation.
    std::uint64_t m_ArmedTicket = 0;

    // Grow-only, for the same reason the engine's internal render resolution is: a request that
    // asks for a smaller image should not pay for a rebuild that the next one undoes.
    bool EnsureBuffer(Pine::Graphics::IFrameBuffer*& buffer, const Pine::Vector2i size)
    {
        if (buffer != nullptr && buffer->GetSize().x >= size.x && buffer->GetSize().y >= size.y)
        {
            return true;
        }

        const auto wanted = buffer == nullptr
            ? size
            : Pine::Vector2i(std::max(buffer->GetSize().x, size.x), std::max(buffer->GetSize().y, size.y));

        if (buffer != nullptr)
        {
            Pine::Graphics::GetGraphicsAPI()->DestroyFrameBuffer(buffer);
            buffer = nullptr;
        }

        buffer = Pine::Graphics::GetGraphicsAPI()->CreateFrameBuffer();
        buffer->Prepare();
        buffer->AttachTextures(wanted.x, wanted.y,
            Pine::Graphics::Buffers::ColorBuffer | Pine::Graphics::Buffers::DepthBuffer);

        if (!buffer->Finish())
        {
            Pine::Graphics::GetGraphicsAPI()->DestroyFrameBuffer(buffer);
            buffer = nullptr;

            return false;
        }

        return true;
    }

    bool EnsureSceneBuffer(const Pine::Vector2i size)
    {
        const auto allocated = EnsureBuffer(m_SceneBuffer, size);

        m_Context.FrameBuffer = m_SceneBuffer;

        return allocated;
    }

    /* GET /asset/preview.png */

    // Reads one optional size parameter, so a typo answers with a 400 rather than silently using
    // the default.
    bool ReadSize(const Request& request, const std::string& name, int& value, std::string& error)
    {
        const auto parameter = ReadIntParameter(request, name);

        if (!parameter.Present)
        {
            return true;
        }

        if (!parameter.Valid || parameter.Value < m_MinimumSize || parameter.Value > m_MaximumSize)
        {
            error = fmt::format("Parameter '{}' must be between {} and {}.", name, m_MinimumSize, m_MaximumSize);

            return false;
        }

        value = parameter.Value;

        return true;
    }

    bool ReadAngle(const Request& request, const std::string& name, float& value, std::string& error)
    {
        const auto parameter = ReadFloatParameter(request, name);

        if (!parameter.Present)
        {
            return true;
        }

        if (!parameter.Valid || !std::isfinite(parameter.Value) ||
            parameter.Value < -360.f || parameter.Value > 360.f)
        {
            error = fmt::format("Parameter '{}' must be a finite angle between -360 and 360 degrees.", name);

            return false;
        }

        value = parameter.Value;

        return true;
    }

    /* POST /render */

    const json& StateSchema()
    {
        static const json schema = {
            { "position", { { "type", "vector3" } } },
            { "rotation", { { "type", "quaternion" } } },
            { "fieldOfView", { { "type", "number" }, { "minimum", 1 }, { "maximum", 175 } } },
            { "nearPlane", { { "type", "number" }, { "minimum", 0.0001f } } },
            { "farPlane", { { "type", "number" }, { "minimum", 0.0001f } } }
        };

        return schema;
    }

    bool FiniteMatrix(const Pine::Matrix4f& matrix)
    {
        for (int column = 0; column < 4; column++)
        {
            for (int row = 0; row < 4; row++)
            {
                if (!std::isfinite(matrix[column][row]))
                {
                    return false;
                }
            }
        }

        return true;
    }

    int ReadDimension(const json& body, const std::string& name, const int fallback)
    {
        if (!body.contains(name))
        {
            return fallback;
        }

        const auto& value = body.at(name);
        const auto path = "/" + name;

        Values::Require(value.is_number_integer(), path, "Expected an integer.");

        const auto size = value.get<std::int64_t>();

        Values::Require(size >= m_MinimumSize && size <= m_MaximumSize, path,
            fmt::format("Expected {} to {} pixels.", m_MinimumSize, m_MaximumSize));

        return static_cast<int>(size);
    }

    Request3D ReadRequest3D(const Request& request)
    {
        Values::Require(request.Body.size() <= 16 * 1024, "", "Render request exceeds 16 KiB.");

        const auto depthLimit = [](int depth, json::parse_event_t, json&)
        {
            Values::Require(depth <= 8, "", "Render JSON nesting exceeds 8 levels.");
            return true;
        };

        const auto body = json::parse(request.Body, depthLimit, false);

        Values::Require(!body.is_discarded(), "", "Request body is not valid JSON.");
        Values::Object(body, "",
            { "position", "rotation", "lookAt", "up", "fieldOfView", "nearPlane", "farPlane", "width", "height" },
            { "position" });
        Values::Require(body.contains("rotation") != body.contains("lookAt"), "/rotation",
            "Specify exactly one of rotation or lookAt.");
        Values::Require(!body.contains("up") || body.contains("lookAt"), "/up", "An up vector requires lookAt.");

        // Validated through the same schema the editor camera uses, so the two routes agree on what
        // a usable pose is.
        json state = {
            { "position", body.at("position") },
            { "fieldOfView", body.value("fieldOfView", json(m_DefaultFieldOfView)) },
            { "nearPlane", body.value("nearPlane", json(m_DefaultNearPlane)) },
            { "farPlane", body.value("farPlane", json(m_DefaultFarPlane)) }
        };

        Values::Properties(state, StateSchema(), "");

        const auto position = Values::Vector3(state.at("position"));

        if (body.contains("lookAt"))
        {
            const glm::dvec3 target = Values::Vector3Field(body.at("lookAt"), "/lookAt");
            const glm::dvec3 up = body.contains("up")
                ? Values::Vector3Field(body.at("up"), "/up")
                : Pine::Vector3f(0.f, 1.f, 0.f);

            state["rotation"] = Pine::SerializationJson::StoreQuaternion(
                Values::LookRotation(target - glm::dvec3(position), up, "/lookAt", body.contains("up")));
        }
        else
        {
            state["rotation"] = body.at("rotation");
        }

        Values::Properties(state, StateSchema(), "");

        Request3D result;

        result.Position = position;
        result.Rotation = Values::Quaternion(state.at("rotation"));
        result.FieldOfView = state.at("fieldOfView").get<float>();
        result.NearPlane = state.at("nearPlane").get<float>();
        result.FarPlane = state.at("farPlane").get<float>();
        result.Size = Pine::Vector2i(ReadDimension(body, "width", m_DefaultSceneWidth),
                                     ReadDimension(body, "height", m_DefaultSceneHeight));

        Values::Require(result.FarPlane > result.NearPlane, "/farPlane",
            "Far plane must exceed near plane in float32.");

        const auto aspect = static_cast<float>(result.Size.x) / static_cast<float>(result.Size.y);
        const auto projection = glm::perspective(glm::radians(result.FieldOfView), aspect, result.NearPlane, result.FarPlane);

        Values::Require(FiniteMatrix(projection), "", "Camera parameters produce a non-finite projection matrix.");

        const auto forward = result.Rotation * Pine::Vector3f(0.f, 0.f, -1.f);
        const auto up = result.Rotation * Pine::Vector3f(0.f, 1.f, 0.f);

        // Camera::BuildViewMatrix adds a unit direction to its float32 position. At large
        // coordinates some axes can lose that offset even when lookAt still returns a finite
        // matrix, silently looking in a different direction from the requested rotation.
        const auto representedForward = (result.Position + forward) - result.Position;

        Values::Require(glm::length(representedForward - forward) <= 0.001f, "/position",
            "Position is too large to represent this view accurately in float32.");
        Values::Require(FiniteMatrix(glm::lookAt(result.Position, result.Position + forward, up)), "/position",
            "Position is too large to represent this view in float32.");

        return result;
    }

    // The capture camera is an ordinary temporary editor entity, so the scene queries and edit
    // operations that already exclude the editor's own camera exclude this one too, and a level
    // load leaves it alone.
    Pine::Camera* GetCaptureCamera()
    {
        if (m_CameraEntity == nullptr)
        {
            m_CameraEntity = Pine::Entity::Create("DebugServerCaptureCamera");

            m_CameraEntity->SetTemporary(true);
            m_CameraEntity->AddComponent<Pine::Camera>();

            m_Context.SceneCamera = m_CameraEntity->GetComponent<Pine::Camera>();
            m_Context.Active = false;

            // Nothing draws a selection outline into this context, so it has no use for a stencil
            // buffer and its attachment does not carry one.
            m_Context.EnableStencilBuffer = false;

            Pine::RenderManager::AddRenderingContextPass(&m_Context);
        }

        return m_CameraEntity->GetComponent<Pine::Camera>();
    }

    void Arm()
    {
        if (m_Queue.empty())
        {
            m_ArmedTicket = 0;
            m_Context.Active = false;

            return;
        }

        const auto armed = m_Queue.front();

        m_Queue.pop_front();

        const auto camera = GetCaptureCamera();
        const auto transform = m_CameraEntity->GetTransform();

        transform->SetLocalPosition(armed.Position);
        transform->SetLocalRotation(armed.Rotation);

        camera->SetFieldOfView(armed.FieldOfView);
        camera->SetNearPlane(armed.NearPlane);
        camera->SetFarPlane(armed.FarPlane);
        camera->SetOverrideAspectRatio(static_cast<float>(armed.Size.x) / static_cast<float>(armed.Size.y));

        // Matching the Level viewport, so a capture looks like the same scene seen from elsewhere
        // rather than a differently coloured one. The skybox is assigned per frame by RenderManager.
        m_Context.ClearColor = Editor::RenderHandler::GetLevelRenderingContext()->ClearColor;
        m_Context.Size = Pine::Vector2f(armed.Size);
        m_Context.Active = true;

        m_ArmedTicket = armed.Ticket;
    }

    // Derived from what was asked for rather than read back off the camera, so the reply describes
    // the capture even though the camera has already been handed to whatever is queued next.
    json DescribeCamera(const Request3D& settings)
    {
        return {
            { "position", Pine::SerializationJson::StoreVector3(settings.Position) },
            { "rotation", Pine::SerializationJson::StoreQuaternion(settings.Rotation) },
            { "fieldOfView", settings.FieldOfView },
            { "nearPlane", settings.NearPlane },
            { "farPlane", settings.FarPlane },
            { "forward", Pine::SerializationJson::StoreVector3(settings.Rotation * Pine::Vector3f(0.f, 0.f, -1.f)) },
            { "up", Pine::SerializationJson::StoreVector3(settings.Rotation * Pine::Vector3f(0.f, 1.f, 0.f)) }
        };
    }

    Response Complete(const Request3D& settings)
    {
        // Captures render one per frame. A request whose turn has not come yet waits for another
        // frame rather than reading back somebody else's viewpoint.
        if (m_ArmedTicket != settings.Ticket)
        {
            Response waiting;

            waiting.Resume = [settings] { return Complete(settings); };

            return waiting;
        }

        m_ArmedTicket = 0;
        m_Context.Active = false;

        if (Pine::Entities::GetSceneGeneration() != settings.SceneGeneration)
        {
            return Error(409, "Scene was replaced before the capture was rendered. Request it again.");
        }

        std::vector<std::uint8_t> png;

        if (!Screenshot::CapturePng(&m_Context, 0, png))
        {
            return Error(500, "The capture context rendered but could not be read back.");
        }

        return { 200, {
            { "frame", Observation::FrameIdentity() },
            { "viewport", { { "view", "capture" }, { "width", settings.Size.x }, { "height", settings.Size.y } } },
            { "camera", DescribeCamera(settings) },
            { "image", Screenshot::DescribeImage(png, settings.Size.x, settings.Size.y) }
        } };
    }
}

Editor::DebugServer::Response Editor::DebugServer::Capture::Preview(const Request& request)
{
    Response error;

    const auto asset = Catalog::Resolve(request, error);

    if (asset == nullptr)
    {
        return error;
    }

    if (!Editor::AssetPreview::Supports(asset))
    {
        return Error(409, fmt::format(
            "'{}' is a {} asset; only Model and Material assets have a preview.",
            asset->GetPath(), Pine::AssetTypeToString(asset->GetType())));
    }

    int width = m_DefaultPreviewSize;
    int height = m_DefaultPreviewSize;
    float yaw = Editor::AssetPreview::DefaultViewAngle.x;
    float pitch = Editor::AssetPreview::DefaultViewAngle.y;
    std::string problem;

    if (!ReadSize(request, "width", width, problem) ||
        !ReadSize(request, "height", height, problem) ||
        !ReadAngle(request, "yaw", yaw, problem) ||
        !ReadAngle(request, "pitch", pitch, problem))
    {
        return Error(400, problem);
    }

    const auto size = Pine::Vector2i(width, height);

    if (!EnsureBuffer(m_PreviewBuffer, size))
    {
        return Error(500, fmt::format("Could not allocate a {}x{} preview buffer.", width, height));
    }

    Editor::AssetPreview::Options options;

    options.Size = size;
    options.ViewAngle = Pine::Vector2f(yaw, pitch);

    // Opaque, unlike the asset browser's icons: a transparent preview reads as an empty image in
    // most viewers, and a mid grey keeps both dark and light assets legible.
    options.Background = Pine::Color(96, 96, 96, 255);

    // Far dimmer than the icon defaults. Those are tuned to produce a bright silhouette at 64x64,
    // and at any size worth looking at they clip every surface of a white-material asset to the
    // same flat white - hiding exactly the shape the preview was asked for. Measured against the
    // engine's white-material cube, these leave the lit faces near 200/255 with the three of them
    // clearly apart, so a brighter asset still has headroom.
    options.Ambient = Pine::Vector3f(0.38f);
    options.LightIntensity = 1.f;

    if (!Editor::AssetPreview::Render(asset, options, m_PreviewBuffer))
    {
        return Error(409, fmt::format("'{}' has no geometry to preview.", asset->GetPath()));
    }

    std::vector<std::uint8_t> png;

    if (!Screenshot::CapturePng(m_PreviewBuffer, size, 0, png))
    {
        return Error(500, "The preview rendered but could not be read back.");
    }

    Response response;

    response.Binary = std::move(png);
    response.ContentType = "image/png";

    return response;
}

Editor::DebugServer::Response Editor::DebugServer::Capture::Scene(const Request& request)
{
    Request3D settings;

    try
    {
        settings = ReadRequest3D(request);
    }
    catch (const Values::ValidationError& exception)
    {
        return { 400, { { "error", exception.what() }, { "path", exception.Path } } };
    }

    if (m_Queue.size() >= m_MaximumQueuedCaptures)
    {
        return Error(409, "Too many captures are already queued; one renders per frame. Retry shortly.");
    }

    if (!EnsureSceneBuffer(settings.Size))
    {
        return Error(500, fmt::format("Could not allocate a {}x{} capture buffer.", settings.Size.x, settings.Size.y));
    }

    // Creates the camera and registers the context on first use, so an editor that never captures
    // pays nothing for this route existing.
    GetCaptureCamera();

    settings.Ticket = m_NextTicket++;
    settings.SceneGeneration = Pine::Entities::GetSceneGeneration();

    m_Queue.push_back(settings);

    Response response;

    response.Resume = [settings] { return Complete(settings); };

    return response;
}

void Editor::DebugServer::Capture::OnRender(Pine::RenderingContext*, const Pine::RenderStage stage)
{
    if (stage == Pine::RenderStage::PreRender)
    {
        Arm();
    }
}

void Editor::DebugServer::Capture::Shutdown()
{
    m_Queue.clear();
    m_ArmedTicket = 0;
    m_Context.Active = false;

    // The camera and the context pass are created together, so one stands for both. The entity
    // itself belongs to Entities and goes with the rest of the scene at engine shutdown.
    if (m_CameraEntity != nullptr)
    {
        Pine::RenderManager::RemoveRenderingContextPass(&m_Context);

        m_CameraEntity = nullptr;
    }

    for (auto buffer : { &m_PreviewBuffer, &m_SceneBuffer })
    {
        if (*buffer != nullptr)
        {
            Pine::Graphics::GetGraphicsAPI()->DestroyFrameBuffer(*buffer);
            *buffer = nullptr;
        }
    }

    m_Context.FrameBuffer = nullptr;
    m_Context.SceneCamera = nullptr;
}
