#include "Spatial.hpp"

#include <cmath>
#include <vector>

#include "../Editing/Values/Values.hpp"
#include "Pine/World/Components/ModelRenderer/ModelRenderer.hpp"
#include "Pine/World/Components/TerrainRenderer/TerrainRendererComponent.hpp"
#include "Pine/World/Entities/Entities.hpp"

namespace
{
    using nlohmann::json;
    using namespace Editor::DebugServer;
    namespace Values = Editing::Values;

    json StoreVector(const glm::dvec3& value)
    {
        for (int axis = 0; axis < 3; axis++)
        {
            Values::Require(std::isfinite(value[axis]), "/entities", "Entity spatial state is not finite.");
        }
        return { { "x", value.x }, { "y", value.y }, { "z", value.z } };
    }

    json StoreTransform(const Pine::Vector3f& position, const Pine::Quaternion& rotation, const Pine::Vector3f& scale)
    {
        Values::Require(std::isfinite(rotation.w), "/entities", "Entity rotation is not finite.");
        auto quaternion = StoreVector(glm::dvec3(rotation.x, rotation.y, rotation.z));
        quaternion["w"] = rotation.w;
        return { { "position", StoreVector(position) }, { "rotation", quaternion }, { "scale", StoreVector(scale) } };
    }

    json DescribeBounds(const Spatial::Bounds& bounds)
    {
        if (bounds.Empty)
        {
            return nullptr;
        }
        return {
            { "min", StoreVector(bounds.Min) }, { "max", StoreVector(bounds.Max) },
            { "center", StoreVector((bounds.Min + bounds.Max) * 0.5) },
            { "dimensions", StoreVector(bounds.Max - bounds.Min) }
        };
    }

    Spatial::Bounds Measure(Pine::Entity* root, const bool includeChildren)
    {
        Spatial::Bounds bounds;
        std::vector<Pine::Entity*> pending = { root };
        while (!pending.empty())
        {
            const auto entity = pending.back();
            pending.pop_back();
            if (entity->GetTemporary())
            {
                continue;
            }

            Spatial::AddModelBounds(entity, bounds);
            const auto renderer = entity->GetComponent<Pine::TerrainRendererComponent>();
            const auto terrain = renderer != nullptr ? renderer->GetTerrain() : nullptr;
            if (terrain != nullptr)
            {
                // Terrain rendering applies translation only. Chunk bounds track the height
                // field immediately after sculpting and exclude the visual crack-hiding skirts.
                const glm::dvec3 position = entity->GetTransform()->GetPosition();
                for (const auto& chunk : terrain->GetChunks())
                {
                    bounds.Include(position + glm::dvec3(chunk.BoundsMin));
                    bounds.Include(position + glm::dvec3(chunk.BoundsMax));
                }
            }

            if (includeChildren)
            {
                const auto& children = entity->GetChildren();
                pending.insert(pending.end(), children.begin(), children.end());
            }
        }
        return bounds;
    }
}

void Editor::DebugServer::Spatial::Bounds::Include(const glm::dvec3& point)
{
    for (int axis = 0; axis < 3; axis++)
    {
        Values::Require(std::isfinite(point[axis]), "/entities", "Entity bounds are not finite.");
    }
    Min = glm::min(Min, point);
    Max = glm::max(Max, point);
    Empty = false;
}

bool Editor::DebugServer::Spatial::AddModelBounds(Pine::Entity* entity, Bounds& bounds)
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
    return hasGeometry;
}

Editor::DebugServer::Response Editor::DebugServer::Spatial::Query(const Request& request)
{
    try
    {
        Values::Require(request.Parameters.empty(), "", "Spatial queries do not accept query parameters.");
        Values::Require(request.Body.size() <= 16 * 1024, "", "Spatial request exceeds 16 KiB.");
        const auto depthLimit = [](int depth, json::parse_event_t, json&)
        {
            Values::Require(depth <= 8, "", "Spatial JSON nesting exceeds 8 levels.");
            return true;
        };
        const auto body = json::parse(request.Body, depthLimit, false);
        Values::Require(!body.is_discarded(), "", "Request body is not valid JSON.");
        Values::Object(body, "", { "entities", "includeChildren" }, { "entities" });
        const auto& references = body.at("entities");
        Values::Require(references.is_array() && !references.empty() && references.size() <= 128,
            "/entities", "Expected 1 to 128 entity references.");
        if (body.contains("includeChildren"))
        {
            Values::Require(body.at("includeChildren").is_boolean(), "/includeChildren", "Expected a boolean.");
        }
        const bool includeChildren = body.value("includeChildren", true);

        auto entities = json::array();
        Bounds combined;
        for (std::size_t index = 0; index < references.size(); index++)
        {
            const auto path = "/entities/" + std::to_string(index);
            Values::Object(references[index], path, { "id" }, { "id" });
            const auto entity = Pine::Entities::Find(Values::Id(references[index].at("id"), path + "/id"));
            Values::Require(entity != nullptr, path + "/id", "Entity does not exist.");
            for (auto ancestor = entity; ancestor != nullptr; ancestor = ancestor->GetParent())
            {
                Values::Require(!ancestor->GetTemporary(), path + "/id", "Expected a scene entity, not an editor entity.");
            }

            const auto bounds = Measure(entity, includeChildren);
            if (!bounds.Empty)
            {
                combined.Include(bounds.Min);
                combined.Include(bounds.Max);
            }
            const auto transform = entity->GetTransform();
            entities.push_back({
                { "id", entity->GetId().ToString() }, { "bounds", DescribeBounds(bounds) },
                { "localTransform", StoreTransform(transform->GetLocalPosition(), transform->GetLocalRotation(), transform->GetLocalScale()) },
                { "worldTransform", StoreTransform(transform->GetPosition(), transform->GetRotation(), transform->GetScale()) },
                { "forward", StoreVector(transform->GetForward()) },
                { "right", StoreVector(transform->GetRight()) }, { "up", StoreVector(transform->GetUp()) }
            });
        }
        return { 200, {
            { "sceneGeneration", Pine::Entities::GetSceneGeneration() },
            { "includeChildren", includeChildren }, { "entities", entities },
            { "combinedBounds", DescribeBounds(combined) }
        } };
    }
    catch (const Values::ValidationError& exception)
    {
        return { 400, { { "error", exception.what() }, { "path", exception.Path } } };
    }
}
