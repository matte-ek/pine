#include "Queries.hpp"

#include <algorithm>
#include <cmath>
#include <unordered_map>
#include <unordered_set>

#include "../Spatial.hpp"
#include "../../Editing/Values/Values.hpp"
#include "Other/PlayHandler/PlayHandler.hpp"
#include "Pine/World/Components/ModelRenderer/ModelRenderer.hpp"
#include "Pine/World/Components/TerrainRenderer/TerrainRendererComponent.hpp"
#include "Pine/World/Entities/Entities.hpp"

namespace
{
    using nlohmann::json;
    using namespace Editor::DebugServer;
    namespace Values = Editing::Values;

    constexpr std::size_t MaximumResults = 128;
    constexpr std::size_t MaximumGeometryBytes = 64 * 1024 * 1024;
    constexpr std::size_t MaximumTriangleTests = 2 * 1024 * 1024;

    json Parse(const Request& request)
    {
        Values::Require(request.Parameters.empty(), "", "Spatial queries do not accept query parameters.");
        Values::Require(request.Body.size() <= 16 * 1024, "", "Spatial request exceeds 16 KiB.");
        const auto depthLimit = [](int depth, json::parse_event_t, json&)
        {
            Values::Require(depth <= 8, "", "Spatial JSON nesting exceeds 8 levels.");
            return true;
        };
        auto body = json::parse(request.Body, depthLimit, false);
        Values::Require(!body.is_discarded(), "", "Request body is not valid JSON.");
        return body;
    }

    double Number(const json& value, const std::string& path)
    {
        Values::Require(value.is_number(), path, "Expected a finite number.");
        const auto number = value.get<double>();
        Values::Require(std::isfinite(number) && std::abs(number) <= 1e12,
            path, "Expected a finite number in [-1e12, 1e12].");
        return number;
    }

    glm::dvec3 Vector(const json& value, const std::string& path)
    {
        Values::Object(value, path, { "x", "y", "z" }, { "x", "y", "z" });
        return { Number(value.at("x"), path + "/x"), Number(value.at("y"), path + "/y"),
            Number(value.at("z"), path + "/z") };
    }

    json StoreVector(const glm::dvec3& value)
    {
        return { { "x", value.x }, { "y", value.y }, { "z", value.z } };
    }

    struct Filter
    {
        bool IncludeInactive = false;
        std::unordered_set<Pine::Entity*> Excluded;

        explicit Filter(const json& body)
        {
            if (body.contains("includeInactive"))
            {
                Values::Require(body.at("includeInactive").is_boolean(), "/includeInactive", "Expected a boolean.");
                IncludeInactive = body.at("includeInactive").get<bool>();
            }
            if (!body.contains("exclude"))
            {
                return;
            }
            const auto& references = body.at("exclude");
            Values::Require(references.is_array() && references.size() <= 128,
                "/exclude", "Expected at most 128 entity references.");
            for (std::size_t index = 0; index < references.size(); index++)
            {
                const auto path = "/exclude/" + std::to_string(index);
                Values::Object(references[index], path, { "id" }, { "id" });
                const auto entity = Pine::Entities::Find(Values::Id(references[index].at("id"), path + "/id"));
                Values::Require(entity != nullptr, path + "/id", "Entity does not exist.");
                for (auto ancestor = entity; ancestor != nullptr; ancestor = ancestor->GetParent())
                {
                    Values::Require(!ancestor->GetTemporary(), path + "/id", "Expected a scene entity, not an editor entity.");
                }
                Excluded.insert(entity);
            }
        }

        bool Includes(Pine::Entity* entity) const
        {
            for (auto ancestor = entity; ancestor != nullptr; ancestor = ancestor->GetParent())
            {
                if (ancestor->GetTemporary() || Excluded.count(ancestor) != 0)
                {
                    return false;
                }
            }
            // Match IsWorldEnabled: Pine's active flag belongs to the entity itself,
            // rather than implicitly disabling its descendants.
            return IncludeInactive || entity->GetActive();
        }
    };

    struct Ray
    {
        glm::dvec3 Origin;
        glm::dvec3 Direction;
        double MaximumDistance;
        json Hit = nullptr;

        void Consider(const double distance, const glm::dvec3& position, glm::dvec3 normal,
            Pine::Component* component, Pine::Asset* asset, const char* geometry, const json& meshIndex)
        {
            if (distance < 0 || distance > MaximumDistance || (!Hit.is_null() && distance >= Hit.at("distance").get<double>()))
            {
                return;
            }
            Values::Require(std::isfinite(distance) && std::isfinite(glm::length(position)) &&
                std::isfinite(glm::length(normal)) && glm::length(normal) > 0,
                "/entities", "Surface intersection is not finite.");
            normal = glm::normalize(normal);
            if (glm::dot(normal, Direction) > 0)
            {
                normal = -normal;
            }
            Hit = {
                { "entity", component->GetParent()->GetId().ToString() }, { "component", component->GetId().ToString() },
                { "asset", asset->GetUId().ToString() }, { "geometry", geometry }, { "meshIndex", meshIndex },
                { "distance", distance }, { "position", StoreVector(position) }, { "normal", StoreVector(normal) }
            };
        }

        void Triangle(const glm::dvec3& first, const glm::dvec3& second, const glm::dvec3& third,
            Pine::ModelRenderer* renderer, const std::size_t meshIndex)
        {
            const auto firstEdge = second - first;
            const auto secondEdge = third - first;
            const auto normal = glm::cross(firstEdge, secondEdge);
            const auto normalLength = glm::length(normal);
            const auto perpendicular = glm::cross(Direction, secondEdge);
            const auto determinant = glm::dot(firstEdge, perpendicular);
            // Relative tolerance keeps small and heavily scaled triangles usable.
            // Degenerate triangles and rays in the face plane have no point hit.
            if (normalLength == 0 || std::abs(determinant) <= 1e-12 * normalLength)
            {
                return;
            }
            const auto toOrigin = Origin - first;
            const auto firstWeight = glm::dot(toOrigin, perpendicular) / determinant;
            const auto originCross = glm::cross(toOrigin, firstEdge);
            const auto secondWeight = glm::dot(Direction, originCross) / determinant;
            constexpr double edgeTolerance = 1e-10;
            if (firstWeight < -edgeTolerance || secondWeight < -edgeTolerance ||
                firstWeight + secondWeight > 1 + edgeTolerance)
            {
                return;
            }
            const auto distance = glm::dot(secondEdge, originCross) / determinant;
            Consider(distance, Origin + Direction * distance, normal, renderer, renderer->GetModel(), "model-surface", meshIndex);
        }
    };

    struct MeshGeometry
    {
        std::vector<Pine::Vector3f> Vertices;
        std::vector<std::uint32_t> Indices;
    };

    // One request owns its readbacks. A later edit or re-import can never leave
    // stale cached geometry, and repeated instances share only the readback cost.
    struct GeometryReader
    {
        std::unordered_map<Pine::Mesh*, MeshGeometry> Meshes;
        std::size_t Bytes = 0;
        std::size_t TriangleTests = 0;

        const MeshGeometry& Read(Pine::Mesh* mesh)
        {
            const std::size_t count = mesh->GetRenderCount();
            if (count / 3 > MaximumTriangleTests - TriangleTests)
            {
                throw std::runtime_error("Raycast exceeds 2,097,152 model triangle tests. Exclude unrelated hierarchies.");
            }
            TriangleTests += count / 3;
            const auto found = Meshes.find(mesh);
            if (found != Meshes.end())
            {
                return found->second;
            }
            const auto bytes = static_cast<std::size_t>(mesh->GetVertexCount()) * sizeof(Pine::Vector3f)
                + (mesh->HasElementBuffer() ? count * sizeof(std::uint32_t) : 0);
            if (bytes > MaximumGeometryBytes - Bytes)
            {
                throw std::runtime_error("Raycast exceeds 64 MiB of model geometry. Exclude unrelated hierarchies.");
            }
            Bytes += bytes;
            auto& geometry = Meshes[mesh];
            if (!mesh->ReadGeometry(geometry.Vertices, geometry.Indices))
            {
                throw std::runtime_error("Model mesh geometry is unavailable for readback.");
            }
            for (const auto& vertex : geometry.Vertices)
            {
                Values::Require(std::isfinite(vertex.x) && std::isfinite(vertex.y) && std::isfinite(vertex.z),
                    "/entities", "Model vertex is not finite.");
            }
            for (const auto index : geometry.Indices)
            {
                Values::Require(index < geometry.Vertices.size(), "/entities", "Model triangle index is outside its vertex buffer.");
            }
            return geometry;
        }
    };

    void RaycastModel(Ray& ray, Pine::ModelRenderer* renderer, GeometryReader& reader)
    {
        const auto model = renderer->GetModel();
        if (model == nullptr)
        {
            return;
        }
        const auto& meshes = model->GetMeshes();
        const auto selected = renderer->GetModelMeshIndex();
        Values::Require(selected == -1 || (selected >= 0 && static_cast<std::size_t>(selected) < meshes.size()),
            "/entities", "A ModelRenderer has an invalid mesh index.");
        const auto transform = renderer->GetTransform();
        const glm::dvec3 position = transform->GetPosition();
        const glm::dquat rotation = transform->GetRotation();
        const glm::dvec3 scale = transform->GetScale();
        Values::Require(std::isfinite(glm::length(position)) && std::isfinite(glm::length(rotation)) &&
            std::isfinite(glm::length(scale)), "/entities", "Model transform is not finite.");

        for (std::size_t meshIndex = 0; meshIndex < meshes.size(); meshIndex++)
        {
            if (selected != -1 && static_cast<std::size_t>(selected) != meshIndex)
            {
                continue;
            }
            const auto mesh = meshes[meshIndex];
            const auto& geometry = reader.Read(mesh);
            const auto worldVertex = [&](const std::size_t index)
            {
                const auto vertex = mesh->HasElementBuffer() ? geometry.Indices[index] : index;
                return position + rotation * (glm::dvec3(geometry.Vertices[vertex]) * scale);
            };
            for (std::size_t index = 0; index + 2 < mesh->GetRenderCount(); index += 3)
            {
                ray.Triangle(worldVertex(index), worldVertex(index + 1), worldVertex(index + 2), renderer, meshIndex);
            }
        }
    }

    void RaycastTerrain(Ray& ray, Pine::TerrainRendererComponent* renderer)
    {
        const auto terrain = renderer->GetTerrain();
        if (terrain == nullptr)
        {
            return;
        }
        const glm::dvec3 position = renderer->GetTransform()->GetPosition();
        Values::Require(std::isfinite(glm::length(position)), "/entities", "Terrain position is not finite.");
        const glm::dvec3 localOrigin = ray.Origin - position;
        const Pine::Vector3f origin = localOrigin;
        Values::Require(std::isfinite(glm::length(origin)), "/entities", "Terrain ray exceeds supported coordinates.");
        const auto hit = terrain->Raycast(origin, Pine::Vector3f(ray.Direction));
        if (hit.has_value())
        {
            ray.Consider(hit->Distance, position + glm::dvec3(hit->Position), hit->Normal,
                renderer, terrain, "terrain-surface", nullptr);
        }
    }
}

Editor::DebugServer::Response Editor::DebugServer::Spatial::Queries::Overlap(const Request& request)
{
    try
    {
        const auto body = Parse(request);
        Values::Object(body, "", { "bounds", "exclude", "includeInactive", "limit" }, { "bounds" });
        const auto& bounds = body.at("bounds");
        Values::Object(bounds, "/bounds", { "min", "max" }, { "min", "max" });
        const auto minimum = Vector(bounds.at("min"), "/bounds/min");
        const auto maximum = Vector(bounds.at("max"), "/bounds/max");
        Values::Require(glm::all(glm::lessThanEqual(minimum, maximum)), "/bounds", "Expected min <= max on every axis.");
        std::size_t limit = MaximumResults;
        if (body.contains("limit"))
        {
            const auto& value = body.at("limit");
            Values::Require(value.is_number_integer() && value >= 1 && value <= MaximumResults,
                "/limit", "Expected an integer from 1 to 128.");
            limit = value.get<std::size_t>();
        }
        const Filter filter(body);
        if (PlayHandler::GetGameState() != PlayHandler::EditorGameState::Stopped)
        {
            return Error(409, "Spatial intersection queries require stopped edit mode.");
        }

        auto entities = json::array();
        std::size_t total = 0;
        for (const auto entity : Pine::Entities::GetList())
        {
            if (!filter.Includes(entity))
            {
                continue;
            }
            Bounds measured;
            AddModelBounds(entity, measured, filter.IncludeInactive);
            AddTerrainBounds(entity, measured, filter.IncludeInactive);
            if (measured.Empty || glm::any(glm::lessThan(measured.Max, minimum)) ||
                glm::any(glm::greaterThan(measured.Min, maximum)))
            {
                continue;
            }
            ++total;
            if (entities.size() < limit)
            {
                entities.push_back({ { "id", entity->GetId().ToString() },
                    { "bounds", { { "min", StoreVector(measured.Min) }, { "max", StoreVector(measured.Max) } } } });
            }
        }
        return { 200, {
            { "sceneGeneration", Pine::Entities::GetSceneGeneration() }, { "geometry", "world-bounds" },
            { "entities", entities }, { "total", total }, { "truncated", total > entities.size() }
        } };
    }
    catch (const Values::ValidationError& exception)
    {
        return { 400, { { "error", exception.what() }, { "path", exception.Path } } };
    }
}

Editor::DebugServer::Response Editor::DebugServer::Spatial::Queries::Raycast(const Request& request)
{
    try
    {
        const auto body = Parse(request);
        Values::Object(body, "", { "origin", "direction", "maxDistance", "exclude", "includeInactive" },
            { "origin", "direction", "maxDistance" });
        const auto origin = Vector(body.at("origin"), "/origin");
        const auto direction = Vector(body.at("direction"), "/direction");
        const auto length = std::hypot(direction.x, direction.y, direction.z);
        Values::Require(length > 0, "/direction", "Expected a nonzero direction.");
        const auto distance = Number(body.at("maxDistance"), "/maxDistance");
        Values::Require(distance > 0, "/maxDistance", "Expected a positive distance in world units.");
        const Filter filter(body);
        if (PlayHandler::GetGameState() != PlayHandler::EditorGameState::Stopped)
        {
            return Error(409, "Spatial intersection queries require stopped edit mode.");
        }

        Ray ray{ origin, direction / length, distance };
        GeometryReader reader;
        for (const auto entity : Pine::Entities::GetList())
        {
            if (!filter.Includes(entity))
            {
                continue;
            }
            for (const auto component : entity->GetComponents())
            {
                if (!filter.IncludeInactive && !component->GetActive())
                {
                    continue;
                }
                if (component->GetType() == Pine::ComponentType::ModelRenderer)
                {
                    RaycastModel(ray, static_cast<Pine::ModelRenderer*>(component), reader);
                }
                else if (component->GetType() == Pine::ComponentType::TerrainRenderer)
                {
                    RaycastTerrain(ray, static_cast<Pine::TerrainRendererComponent*>(component));
                }
            }
        }
        return { 200, { { "sceneGeneration", Pine::Entities::GetSceneGeneration() },
            { "geometry", "model-surfaces-and-terrain" }, { "hit", ray.Hit } } };
    }
    catch (const Values::ValidationError& exception)
    {
        return { 400, { { "error", exception.what() }, { "path", exception.Path } } };
    }
    catch (const std::runtime_error& exception)
    {
        return Error(409, exception.what());
    }
}
