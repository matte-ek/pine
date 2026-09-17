#include "Picking.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <deque>
#include <memory>

#include "../Editing/Values/Values.hpp"
#include "Other/PlayHandler/PlayHandler.hpp"
#include "Pine/Graphics/Graphics.hpp"
#include "Pine/World/Components/ModelRenderer/ModelRenderer.hpp"
#include "Pine/World/Entities/Entities.hpp"

namespace
{
    using nlohmann::json;
    using namespace Editor::DebugServer;
    namespace Values = Editing::Values;
    namespace Graphics = Pine::Graphics;
    using Clock = std::chrono::steady_clock;

    constexpr std::size_t MaximumPixels = 2 * 1024 * 1024;
    constexpr std::size_t MaximumMeshes = 16384;
    constexpr std::size_t MaximumCaptures = 4;
    constexpr std::size_t MaximumRetainedBytes = 64 * 1024 * 1024;
    constexpr auto Retention = std::chrono::seconds(120);

    struct MeshIdentity
    {
        Pine::UId Entity;
        Pine::UId Component;
        Pine::UId Model;
        int Index;
    };

    struct CaptureData
    {
        std::string Id;
        json Frame;
        int Width;
        int Height;
        glm::dmat4 InverseViewProjection;
        Clock::time_point Created;
        std::vector<MeshIdentity> Meshes;
        std::vector<Pine::Vector4f> Surfaces;
        std::vector<float> Depth;

        std::size_t Bytes() const
        {
            return Surfaces.size() * sizeof(Pine::Vector4f) + Depth.size() * sizeof(float)
                + Meshes.size() * sizeof(MeshIdentity);
        }
    };

    std::deque<CaptureData> m_Captures;
    Graphics::IShaderProgram* m_Program = nullptr;

    // Two half-float channels hold an octahedral normal. The other two store an
    // integer mesh slot in base 1024, whose digits are exact even in RGBA16F.
    constexpr const char* VertexShader = R"glsl(#version 330 core
layout(location = 0) in vec3 position;
uniform mat4 model;
uniform mat4 viewProjection;
out vec3 worldPosition;
void main()
{
    vec4 world = model * vec4(position, 1.0);
    worldPosition = world.xyz;
    gl_Position = viewProjection * world;
}
)glsl";

    constexpr const char* FragmentShader = R"glsl(#version 330 core
in vec3 worldPosition;
uniform int meshSlot;
out vec4 surface;
vec2 signNotZero(vec2 value)
{
    return vec2(value.x >= 0.0 ? 1.0 : -1.0, value.y >= 0.0 ? 1.0 : -1.0);
}
void main()
{
    vec3 normal = normalize(cross(dFdx(worldPosition), dFdy(worldPosition)));
    normal /= abs(normal.x) + abs(normal.y) + abs(normal.z);
    vec2 encoded = normal.xy;
    if (normal.z < 0.0)
        encoded = (1.0 - abs(encoded.yx)) * signNotZero(encoded);
    surface = vec4(encoded, float(meshSlot % 1024), float(meshSlot / 1024));
}
)glsl";

    bool PrepareProgram()
    {
        if (m_Program != nullptr)
        {
            return true;
        }
        const auto api = Graphics::GetGraphicsAPI();
        m_Program = api->CreateShaderProgram();
        if (!m_Program->CompileAndLoadShader(VertexShader, Graphics::ShaderType::Vertex)
            || !m_Program->CompileAndLoadShader(FragmentShader, Graphics::ShaderType::Fragment)
            || !m_Program->LinkProgram())
        {
            api->DestroyShaderProgram(m_Program);
            m_Program = nullptr;
            return false;
        }
        return true;
    }

    void RemoveExpired()
    {
        const auto now = Clock::now();
        const auto generation = Pine::Entities::GetSceneGeneration();
        m_Captures.erase(std::remove_if(m_Captures.begin(), m_Captures.end(), [&](const CaptureData& capture)
        {
            return now - capture.Created >= Retention || capture.Frame.at("sceneGeneration") != generation;
        }), m_Captures.end());
    }

    bool IsSceneEntity(const Pine::Entity* entity)
    {
        for (auto ancestor = entity; ancestor != nullptr; ancestor = ancestor->GetParent())
        {
            if (ancestor->GetTemporary())
            {
                return false;
            }
        }
        return true;
    }

    struct Draw
    {
        Pine::Mesh* Mesh;
        Pine::Matrix4f Transform;
        MeshIdentity Identity;
    };

    std::vector<Draw> CollectDraws(const Pine::RenderingContext& context)
    {
        std::vector<Draw> draws;
        for (const auto& renderer : Pine::Components::Get<Pine::ModelRenderer>())
        {
            const auto model = renderer.GetModel();
            if (model == nullptr || !IsSceneEntity(renderer.GetParent())
                || !context.Visibility.IsVisible(renderer.GetInternalId()))
            {
                continue;
            }
            const auto& meshes = model->GetMeshes();
            for (std::size_t index = 0; index < meshes.size(); ++index)
            {
                if (renderer.GetModelMeshIndex() >= 0 && renderer.GetModelMeshIndex() != static_cast<int>(index))
                {
                    continue;
                }
                draws.push_back({ meshes[index], renderer.GetTransform()->GetTransformationMatrix(),
                    { renderer.GetParent()->GetId(), renderer.GetId(), model->GetUId(), static_cast<int>(index) } });
                if (draws.size() > MaximumMeshes)
                {
                    return draws;
                }
            }
        }
        return draws;
    }

    struct FrameBufferDeleter
    {
        void operator()(Graphics::IFrameBuffer* buffer) const
        {
            const auto api = Graphics::GetGraphicsAPI();
            api->BindFrameBuffer(nullptr);
            api->DestroyFrameBuffer(buffer);
            // The following ImGui pass and next scene pass establish their own state.
            api->ResetInternalChangeTracking();
        }
    };

    json StoreVector(const glm::dvec3& vector)
    {
        return { { "x", vector.x }, { "y", vector.y }, { "z", vector.z } };
    }

    glm::dvec3 DecodeNormal(const Pine::Vector4f& surface)
    {
        glm::dvec3 normal(surface.x, surface.y, 1.0 - std::abs(surface.x) - std::abs(surface.y));
        if (normal.z < 0.0)
        {
            const auto x = normal.x;
            normal.x = (1.0 - std::abs(normal.y)) * (x >= 0.0 ? 1.0 : -1.0);
            normal.y = (1.0 - std::abs(x)) * (normal.y >= 0.0 ? 1.0 : -1.0);
        }
        return glm::normalize(normal);
    }

    int PixelCoordinate(const json& value, const std::string& path)
    {
        Values::Require(value.is_number_integer() && value >= 0 && value < 4096, path,
            "Expected a nonnegative integer pixel coordinate below 4096.");
        return value.get<int>();
    }
}

Editor::DebugServer::Response Editor::DebugServer::Picking::Capture(
    const Pine::RenderingContext& context, const Pine::Matrix4f& viewProjection,
    const int width, const int height, const json& frame)
{
    RemoveExpired();
    if (PlayHandler::GetGameState() != PlayHandler::EditorGameState::Stopped)
    {
        return Error(409, "Picking captures require stopped edit mode.");
    }
    const auto pixelCount = static_cast<std::size_t>(width) * height;
    if (width < 1 || height < 1 || width > 4096 || height > 4096 || pixelCount > MaximumPixels)
    {
        return Error(409, "Picking capture exceeds 4096 per dimension or 2097152 pixels. Request a smaller observation width.");
    }
    const auto draws = CollectDraws(context);
    if (draws.size() > MaximumMeshes)
    {
        return Error(409, "Picking capture exceeds 16384 visible model meshes.");
    }
    if (!PrepareProgram())
    {
        return Error(500, "Could not prepare the picking shader.");
    }

    CaptureData capture;
    capture.Id = Pine::UId::New().ToString();
    capture.Frame = frame;
    capture.Width = width;
    capture.Height = height;
    capture.InverseViewProjection = glm::inverse(glm::dmat4(viewProjection));
    for (int column = 0; column < 4; ++column)
    {
        for (int row = 0; row < 4; ++row)
        {
            if (!std::isfinite(capture.InverseViewProjection[column][row]))
            {
                return Error(409, "Capture camera matrix cannot be inverted.");
            }
        }
    }
    capture.Created = Clock::now();
    capture.Surfaces.resize(pixelCount);
    capture.Depth.resize(pixelCount);
    capture.Meshes.reserve(draws.size());

    const auto api = Graphics::GetGraphicsAPI();
    std::unique_ptr<Graphics::IFrameBuffer, FrameBufferDeleter> buffer(api->CreateFrameBuffer());
    buffer->Prepare();
    buffer->AttachTextures(width, height, Graphics::ColorBuffer | Graphics::DepthBuffer, 0, Graphics::TextureFormat::RGBA16F);
    if (!buffer->Finish())
    {
        return Error(500, "Could not prepare the picking framebuffer.");
    }
    buffer->Bind();
    api->SetViewport(Pine::Vector2i(0), Pine::Vector2i(width, height));
    api->SetScissorEnabled(false);
    api->SetDepthBiasEnabled(false);
    api->SetBlendingEnabled(false);
    api->SetStencilTestEnabled(false);
    api->SetDepthTestEnabled(true);
    api->SetDepthFunction(Graphics::TestFunction::Less);
    api->SetFaceCullingEnabled(true);
    api->SetFaceCullingMode(Graphics::FaceCullMode::Back);
    api->SetWireframeEnabled(false);
    api->ClearColor(Pine::Color(0, 0, 0, 0));
    api->ClearBuffers(Graphics::ColorBuffer | Graphics::DepthBuffer);

    m_Program->Use();
    m_Program->GetUniformVariable("viewProjection")->LoadMatrix4(viewProjection);
    const auto modelUniform = m_Program->GetUniformVariable("model");
    const auto slotUniform = m_Program->GetUniformVariable("meshSlot");
    for (const auto& draw : draws)
    {
        capture.Meshes.push_back(draw.Identity);
        modelUniform->LoadMatrix4(draw.Transform);
        slotUniform->LoadInteger(static_cast<int>(capture.Meshes.size()));
        draw.Mesh->GetVertexArray()->Bind();
        if (draw.Mesh->HasElementBuffer())
        {
            api->DrawElements(Graphics::RenderMode::Triangles, draw.Mesh->GetRenderCount());
        }
        else
        {
            api->DrawArrays(Graphics::RenderMode::Triangles, draw.Mesh->GetRenderCount());
        }
    }
    buffer->ReadPixels(Pine::Vector2i(0), Pine::Vector2i(width, height), Graphics::ReadFormat::RGBA,
        Graphics::TextureDataFormat::Float, capture.Surfaces.size() * sizeof(Pine::Vector4f), capture.Surfaces.data());
    buffer->ReadPixels(Pine::Vector2i(0), Pine::Vector2i(width, height), Graphics::ReadFormat::Depth,
        Graphics::TextureDataFormat::Float, capture.Depth.size() * sizeof(float), capture.Depth.data());

    auto retainedBytes = capture.Bytes();
    for (const auto& retained : m_Captures)
    {
        retainedBytes += retained.Bytes();
    }
    while (!m_Captures.empty() && (m_Captures.size() >= MaximumCaptures || retainedBytes > MaximumRetainedBytes))
    {
        retainedBytes -= m_Captures.front().Bytes();
        m_Captures.pop_front();
    }
    json metadata = {
        { "capture", capture.Id }, { "geometry", "model-surfaces" },
        { "width", width }, { "height", height }, { "retentionSeconds", Retention.count() },
        { "maximumCaptures", MaximumCaptures }, { "maximumRetainedBytes", MaximumRetainedBytes }
    };
    m_Captures.push_back(std::move(capture));
    return { 200, metadata };
}

Editor::DebugServer::Response Editor::DebugServer::Picking::Pick(const Request& request)
{
    try
    {
        Values::Require(request.Parameters.empty(), "", "Picking does not accept query parameters.");
        Values::Require(request.Body.size() <= 4096, "", "Picking request exceeds 4 KiB.");
        const auto depthLimit = [](int depth, json::parse_event_t, json&)
        {
            Values::Require(depth <= 4, "", "Picking JSON nesting exceeds 4 levels.");
            return true;
        };
        const auto body = json::parse(request.Body, depthLimit, false);
        Values::Require(!body.is_discarded(), "", "Request body is not valid JSON.");
        Values::Object(body, "", { "capture", "pixel" }, { "capture", "pixel" });
        const auto id = Values::Id(body.at("capture"), "/capture").ToString();
        const auto& pixel = body.at("pixel");
        Values::Object(pixel, "/pixel", { "x", "y" }, { "x", "y" });
        const auto x = PixelCoordinate(pixel.at("x"), "/pixel/x");
        const auto y = PixelCoordinate(pixel.at("y"), "/pixel/y");
        RemoveExpired();
        const auto found = std::find_if(m_Captures.begin(), m_Captures.end(), [&](const CaptureData& capture)
        {
            return capture.Id == id;
        });
        if (found == m_Captures.end())
        {
            return Error(409, "Picking capture is unavailable, expired, evicted or from a replaced scene. Request a new observation with picking enabled.");
        }
        const auto& capture = *found;
        Values::Require(x < capture.Width && y < capture.Height, "/pixel", "Pixel is outside the captured image.");
        const auto index = static_cast<std::size_t>(capture.Height - 1 - y) * capture.Width + x;
        const auto& surface = capture.Surfaces[index];
        const auto slot = static_cast<std::size_t>(surface.z) + static_cast<std::size_t>(surface.w) * 1024;
        json result = { { "capture", id }, { "frame", capture.Frame }, { "pixel", pixel },
            { "geometry", "model-surfaces" }, { "hit", nullptr } };
        if (slot == 0 || slot > capture.Meshes.size())
        {
            return { 200, result };
        }

        // Pixel centres use the returned PNG's top-left origin, independent of the
        // native viewport size. Depth and surface samples share this exact grid.
        const glm::dvec4 clip(2.0 * (x + 0.5) / capture.Width - 1.0,
            1.0 - 2.0 * (y + 0.5) / capture.Height, 2.0 * capture.Depth[index] - 1.0, 1.0);
        const auto homogeneous = capture.InverseViewProjection * clip;
        const glm::dvec3 position = glm::dvec3(homogeneous) / homogeneous.w;
        const auto normal = DecodeNormal(surface);
        if (!std::isfinite(position.x) || !std::isfinite(position.y) || !std::isfinite(position.z)
            || !std::isfinite(normal.x) || !std::isfinite(normal.y) || !std::isfinite(normal.z))
        {
            return Error(409, "The captured surface cannot be reconstructed.");
        }
        const auto& identity = capture.Meshes[slot - 1];
        result["hit"] = {
            { "entity", identity.Entity.ToString() }, { "component", identity.Component.ToString() },
            { "model", identity.Model.ToString() }, { "meshIndex", identity.Index },
            { "position", StoreVector(position) }, { "normal", StoreVector(normal) }
        };
        return { 200, result };
    }
    catch (const Values::ValidationError& exception)
    {
        return { 400, { { "error", exception.what() }, { "path", exception.Path } } };
    }
}

void Editor::DebugServer::Picking::Shutdown()
{
    m_Captures.clear();
    if (m_Program != nullptr)
    {
        Graphics::GetGraphicsAPI()->DestroyShaderProgram(m_Program);
        m_Program = nullptr;
    }
}
