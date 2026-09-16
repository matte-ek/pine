#include "Camera.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

#include "../Editing/Values/Values.hpp"
#include "Other/EditorEntity/EditorEntity.hpp"
#include "Rendering/RenderHandler.hpp"
#include "Pine/Core/Serialization/Json/SerializationJson.hpp"
#include "Pine/World/Components/ModelRenderer/ModelRenderer.hpp"
#include "Pine/World/Entities/Entities.hpp"

namespace
{
    using nlohmann::json;
    using namespace Editor::DebugServer;
    namespace Values = Editing::Values;

    struct Bounds
    {
        glm::dvec3 Min = glm::dvec3(std::numeric_limits<double>::max());
        glm::dvec3 Max = glm::dvec3(std::numeric_limits<double>::lowest());

        void Include(const glm::dvec3& point)
        {
            for (int axis = 0; axis < 3; axis++)
            {
                Values::Require(std::isfinite(point[axis]), "/entities", "Entity bounds are not finite.");
            }
            Min = glm::min(Min, point);
            Max = glm::max(Max, point);
        }
    };

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

    json Parse(const Request& request)
    {
        Values::Require(request.Body.size() <= 16 * 1024, "", "Camera request exceeds 16 KiB.");
        const auto depthLimit = [](int depth, json::parse_event_t, json&)
        {
            Values::Require(depth <= 8, "", "Camera JSON nesting exceeds 8 levels.");
            return true;
        };
        auto body = json::parse(request.Body, depthLimit, false);
        Values::Require(!body.is_discarded(), "", "Request body is not valid JSON.");
        return body;
    }

    Pine::Camera* GetCamera()
    {
        return Editor::LevelEntity::Get()->GetComponent<Pine::Camera>();
    }

    json ReadState()
    {
        const auto camera = GetCamera();
        const auto transform = camera->GetTransform();
        return {
            { "position", Pine::SerializationJson::StoreVector3(transform->GetPosition()) },
            { "rotation", Pine::SerializationJson::StoreQuaternion(transform->GetRotation()) },
            { "fieldOfView", camera->GetFieldOfView() },
            { "nearPlane", camera->GetNearPlane() },
            { "farPlane", camera->GetFarPlane() }
        };
    }

    json Describe()
    {
        const auto context = Editor::RenderHandler::GetLevelRenderingContext();
        const auto transform = GetCamera()->GetTransform();
        return {
            { "state", ReadState() },
            { "forward", Pine::SerializationJson::StoreVector3(transform->GetForward()) },
            { "up", Pine::SerializationJson::StoreVector3(transform->GetUp()) },
            { "viewport", { { "width", context->Size.x }, { "height", context->Size.y }, { "active", context->Active } } },
            { "mouseCaptured", Editor::LevelEntity::GetCaptureMouse() }
        };
    }

    bool IsPerspective3D()
    {
        return !Editor::LevelEntity::GetPerspective2D() && GetCamera()->GetCameraType() == Pine::CameraType::Perspective;
    }

    Response Unavailable()
    {
        return Error(409, "Camera controls require the Level viewport's 3D perspective mode.");
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

    void ValidateState(json& state)
    {
        Values::Properties(state, StateSchema(), "");
        const auto nearPlane = state.at("nearPlane").get<float>();
        const auto farPlane = state.at("farPlane").get<float>();
        Values::Require(farPlane > nearPlane, "/farPlane", "Far plane must exceed near plane in float32.");

        const auto context = Editor::RenderHandler::GetLevelRenderingContext();
        const auto aspect = context->Size.x > 0 && context->Size.y > 0 ? context->Size.x / context->Size.y : 1.f;
        const auto projection = glm::perspective(glm::radians(state.at("fieldOfView").get<float>()), aspect, nearPlane, farPlane);
        Values::Require(FiniteMatrix(projection), "", "Camera parameters produce a non-finite projection matrix.");

        const auto position = Values::Vector3(state.at("position"));
        const auto rotation = Values::Quaternion(state.at("rotation"));
        const auto forward = rotation * Pine::Vector3f(0, 0, -1);
        const auto up = rotation * Pine::Vector3f(0, 1, 0);
        // Camera::BuildViewMatrix adds a unit direction to its float32 position. At large
        // coordinates some axes can lose that offset even when lookAt still returns a finite
        // matrix, silently looking in a different direction from the requested rotation.
        const auto representedForward = (position + forward) - position;
        Values::Require(glm::length(representedForward - forward) <= 0.001f, "/position",
            "Position is too large to represent this view accurately in float32.");
        const auto view = glm::lookAt(position, position + forward, up);
        Values::Require(FiniteMatrix(view), "/position", "Position is too large to represent this view in float32.");
    }

    void Apply(const json& state)
    {
        const auto camera = GetCamera();
        camera->SetFieldOfView(state.at("fieldOfView").get<float>());
        camera->SetNearPlane(state.at("nearPlane").get<float>());
        camera->SetFarPlane(state.at("farPlane").get<float>());
        Editor::LevelEntity::SetView(Values::Vector3(state.at("position")), Values::Quaternion(state.at("rotation")));
    }

    Pine::Vector3f ReadVector(const json& value, const std::string& name)
    {
        json properties = { { name, value } };
        Values::Properties(properties, { { name, { { "type", "vector3" } } } }, "");
        return Values::Vector3(properties.at(name));
    }

    Pine::Quaternion LookRotation(glm::dvec3 direction, glm::dvec3 up, const std::string& path, const bool explicitUp)
    {
        Values::Require(glm::length(direction) > 0, path, "View direction must have nonzero length.");
        Values::Require(glm::length(up) > 0, "/up", "Up vector must have nonzero length.");
        direction = glm::normalize(direction);
        up = glm::normalize(up);

        if (std::abs(glm::dot(direction, up)) > 0.9999)
        {
            Values::Require(!explicitUp, "/up", "Up vector must not be parallel to the view direction.");
            // A top/bottom view still needs a stable orientation when the default world-up is
            // parallel to its direction. Explicitly conflicting inputs are rejected instead.
            up = glm::dvec3(0, 0, -1);
        }

        const auto rotation = glm::quat_cast(glm::transpose(glm::dmat3(glm::lookAt(glm::dvec3(0), direction, up))));
        return glm::normalize(Pine::Quaternion(rotation));
    }

    bool AddEntityBounds(Pine::Entity* entity, const bool includeChildren, Bounds& bounds)
    {
        bool hasGeometry = false;
        for (const auto component : entity->GetComponents())
        {
            if (component->GetType() != Pine::ComponentType::ModelRenderer)
            {
                continue;
            }

            const auto renderer = static_cast<Pine::ModelRenderer*>(component);
            const auto model = renderer->GetModel();
            if (model == nullptr || model->GetMeshes().empty())
            {
                continue;
            }

            auto localMin = model->GetBoundingBoxMin();
            auto localMax = model->GetBoundingBoxMax();
            const auto meshIndex = renderer->GetModelMeshIndex();
            if (meshIndex != -1)
            {
                Values::Require(meshIndex >= 0 && static_cast<std::size_t>(meshIndex) < model->GetMeshes().size(),
                    "/entities", "A selected ModelRenderer has an invalid mesh index.");
                localMin = model->GetMeshes()[meshIndex]->GetBoundingBoxMin();
                localMax = model->GetMeshes()[meshIndex]->GetBoundingBoxMax();
            }

            const auto transform = entity->GetTransform();
            const glm::dvec3 position = transform->GetPosition();
            const glm::dquat rotation = transform->GetRotation();
            const glm::dvec3 scale = transform->GetScale();

            // Match Pine's actual world transform accessors, including its parent semantics.
            // Cached renderer bounds and matrices may still describe the frame before /edit.
            for (int corner = 0; corner < 8; corner++)
            {
                const glm::dvec3 localCorner(
                    corner & 1 ? localMax.x : localMin.x,
                    corner & 2 ? localMax.y : localMin.y,
                    corner & 4 ? localMax.z : localMin.z);
                bounds.Include(position + rotation * (localCorner * scale));
            }
            hasGeometry = true;
        }

        if (includeChildren)
        {
            for (const auto child : entity->GetChildren())
            {
                if (!child->GetTemporary())
                {
                    hasGeometry = AddEntityBounds(child, true, bounds) || hasGeometry;
                }
            }
        }
        return hasGeometry;
    }

    Bounds ReadBounds(const json& body)
    {
        const auto& entities = body.at("entities");
        Values::Require(entities.is_array() && !entities.empty() && entities.size() <= 128,
            "/entities", "Expected 1 to 128 entity references.");
        if (body.contains("includeChildren"))
        {
            Values::Require(body.at("includeChildren").is_boolean(), "/includeChildren", "Expected a boolean.");
        }
        const auto includeChildren = body.value("includeChildren", true);
        Bounds bounds;

        for (std::size_t index = 0; index < entities.size(); index++)
        {
            const auto path = "/entities/" + std::to_string(index);
            Values::Object(entities[index], path, { "id" }, { "id" });
            const auto entity = Pine::Entities::Find(Values::Id(entities[index].at("id"), path + "/id"));
            Values::Require(entity != nullptr, path + "/id", "Entity does not exist.");
            for (auto ancestor = entity; ancestor != nullptr; ancestor = ancestor->GetParent())
            {
                Values::Require(!ancestor->GetTemporary(), path + "/id", "Expected a scene entity, not an editor entity.");
            }

            if (!AddEntityBounds(entity, includeChildren, bounds))
            {
                // Lights, empty entities and unsupported geometry can still be located by pivot.
                bounds.Include(glm::dvec3(entity->GetTransform()->GetPosition()));
            }
        }
        return bounds;
    }

    void FrameBounds(json& state, const Bounds& bounds, const double aspect, const double padding)
    {
        const auto center = (bounds.Min + bounds.Max) * 0.5;
        const glm::dquat rotation = Values::Quaternion(state.at("rotation"));
        const auto forward = rotation * glm::dvec3(0, 0, -1);
        const auto right = rotation * glm::dvec3(1, 0, 0);
        const auto up = rotation * glm::dvec3(0, 1, 0);
        const double tangentY = std::tan(glm::radians(state.at("fieldOfView").get<double>()) * 0.5);
        const double tangentX = tangentY * aspect;
        const double nearPlane = state.at("nearPlane").get<double>();
        const double farPlane = state.at("farPlane").get<double>();

        double distance = nearPlane * 1.01;
        if (glm::length(bounds.Max - bounds.Min) < 0.000001)
        {
            // A pivot has no projected size to fit. Prefer one unit away, or the middle of a
            // shorter clipping range. Small models still use their actual projected extents.
            distance = std::max(distance, std::min(1.0, (nearPlane + farPlane) * 0.5));
        }
        double maxDepthOffset = std::numeric_limits<double>::lowest();
        for (int corner = 0; corner < 8; corner++)
        {
            const glm::dvec3 point(
                corner & 1 ? bounds.Max.x : bounds.Min.x,
                corner & 2 ? bounds.Max.y : bounds.Min.y,
                corner & 4 ? bounds.Max.z : bounds.Min.z);
            const auto offset = point - center;
            const double depthOffset = glm::dot(offset, forward);

            // At camera position center - forward * distance, depth is distance + depthOffset.
            // Solve both perspective inequalities for every corner, accounting for aspect ratio.
            distance = std::max(distance, padding * std::abs(glm::dot(offset, right)) / tangentX - depthOffset);
            distance = std::max(distance, padding * std::abs(glm::dot(offset, up)) / tangentY - depthOffset);
            distance = std::max(distance, nearPlane * 1.01 - depthOffset);
            maxDepthOffset = std::max(maxDepthOffset, depthOffset);
        }

        // Small numerical margin keeps a padding=1 request inside the float32 frustum too.
        distance *= 1.0001;
        Values::Require(distance + maxDepthOffset < farPlane, "/entities",
            "Selection cannot fit within the current clipping planes. Increase farPlane using POST /camera.");

        const auto position = center - forward * distance;
        state["position"] = { { "x", position.x }, { "y", position.y }, { "z", position.z } };
    }

    Response ValidationFailure(const Values::ValidationError& exception)
    {
        return { 400, { { "error", exception.what() }, { "path", exception.Path } } };
    }
}

Editor::DebugServer::Response Editor::DebugServer::Camera::Get(const Request& request)
{
    if (!IsPerspective3D())
    {
        return Unavailable();
    }
    return { 200, Describe() };
}

Editor::DebugServer::Response Editor::DebugServer::Camera::Set(const Request& request)
{
    if (!IsPerspective3D())
    {
        return Unavailable();
    }
    if (Editor::LevelEntity::GetCaptureMouse())
    {
        return Error(409, "Release mouse navigation before changing the editor camera.");
    }

    try
    {
        const auto body = Parse(request);
        Values::Object(body, "", { "position", "rotation", "fieldOfView", "nearPlane", "farPlane", "lookAt", "up" });
        Values::Require(!body.empty(), "", "Expected camera state fields or a lookAt target.");
        Values::Require(!body.contains("rotation") || !body.contains("lookAt"), "/rotation", "Specify rotation or lookAt, not both.");
        Values::Require(!body.contains("up") || body.contains("lookAt"), "/up", "An up vector requires lookAt.");

        auto state = ReadState();
        for (const auto& property : body.items())
        {
            if (StateSchema().contains(property.key()))
            {
                state[property.key()] = property.value();
            }
        }
        Values::Properties(state, StateSchema(), "");

        if (body.contains("lookAt"))
        {
            const glm::dvec3 target = ReadVector(body.at("lookAt"), "lookAt");
            const glm::dvec3 position = Values::Vector3(state.at("position"));
            const glm::dvec3 up = body.contains("up") ? ReadVector(body.at("up"), "up") : Pine::Vector3f(0, 1, 0);
            state["rotation"] = Pine::SerializationJson::StoreQuaternion(LookRotation(target - position, up, "/lookAt", body.contains("up")));
        }

        ValidateState(state);
        Apply(state);
        return { 200, Describe() };
    }
    catch (const Values::ValidationError& exception)
    {
        return ValidationFailure(exception);
    }
}

Editor::DebugServer::Response Editor::DebugServer::Camera::Frame(const Request& request)
{
    if (!IsPerspective3D())
    {
        return Unavailable();
    }
    if (Editor::LevelEntity::GetCaptureMouse())
    {
        return Error(409, "Release mouse navigation before framing entities.");
    }

    const auto context = Editor::RenderHandler::GetLevelRenderingContext();
    if (!context->Active || context->Size.x <= 0 || context->Size.y <= 0)
    {
        return Error(409, "Open the Level viewport before framing entities; its aspect ratio is required.");
    }

    try
    {
        const auto body = Parse(request);
        Values::Object(body, "", { "entities", "includeChildren", "padding", "direction", "up" }, { "entities" });
        Values::Require(!body.contains("up") || body.contains("direction"), "/up", "An up vector requires direction.");
        json options = { { "padding", body.value("padding", json(1.2)) } };
        Values::Properties(options, { { "padding", { { "type", "number" }, { "minimum", 1 }, { "maximum", 10 } } } }, "");

        auto state = ReadState();
        if (body.contains("direction"))
        {
            const glm::dvec3 direction = ReadVector(body.at("direction"), "direction");
            const glm::dvec3 up = body.contains("up") ? ReadVector(body.at("up"), "up") : Pine::Vector3f(0, 1, 0);
            state["rotation"] = Pine::SerializationJson::StoreQuaternion(LookRotation(direction, up, "/direction", body.contains("up")));
        }
        ValidateState(state);
        const auto bounds = ReadBounds(body);
        FrameBounds(state, bounds, static_cast<double>(context->Size.x) / context->Size.y, options.at("padding").get<double>());
        ValidateState(state);
        Apply(state);

        auto response = Describe();
        response["framedBounds"] = {
            { "min", { { "x", bounds.Min.x }, { "y", bounds.Min.y }, { "z", bounds.Min.z } } },
            { "max", { { "x", bounds.Max.x }, { "y", bounds.Max.y }, { "z", bounds.Max.z } } }
        };
        return { 200, response };
    }
    catch (const Values::ValidationError& exception)
    {
        return ValidationFailure(exception);
    }
}
