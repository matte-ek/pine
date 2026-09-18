#include "Inspection.hpp"

#include <algorithm>
#include <cmath>
#include <optional>
#include <vector>

#include "../Editing/Editing.hpp"
#include "../Editing/Values/Values.hpp"
#include "../Spatial/Spatial.hpp"
#include "Pine/World/Entities/Entities.hpp"

namespace
{
    using nlohmann::json;
    using namespace Editor::DebugServer;
    namespace Values = Editing::Values;

    constexpr std::size_t MaximumResults = 128;
    constexpr std::size_t MaximumResponseBytes = 1024 * 1024;

    bool Boolean(const json& object, const char* key, const std::string& path, const bool fallback)
    {
        if (!object.contains(key))
        {
            return fallback;
        }
        Values::Require(object.at(key).is_boolean(), path + "/" + key, "Expected a boolean.");
        return object.at(key).get<bool>();
    }

    std::string Choice(const json& value, const std::string& path,
        const std::initializer_list<const char*> choices)
    {
        const auto name = Values::String(value, path);
        for (const auto choice : choices)
        {
            if (name == choice)
            {
                return name;
            }
        }
        throw Values::ValidationError(path, "Unknown choice.");
    }

    Pine::ComponentType ComponentType(const json& value, const std::string& path)
    {
        const auto name = Values::String(value, path);
        // Use the engine's type names, including components without editing adapters.
        for (std::size_t index = 0; index < Pine::Components::GetComponentTypes().size(); index++)
        {
            const auto type = static_cast<Pine::ComponentType>(index);
            if (name == Pine::ComponentTypeToString(type))
            {
                Values::Require(type != Pine::ComponentType::Collider2D && type != Pine::ComponentType::RigidBody2D &&
                    type != Pine::ComponentType::SpriteRenderer && type != Pine::ComponentType::TilemapRenderer,
                    path, "2D component inspection queries are not supported.");
                return type;
            }
        }
        throw Values::ValidationError(path, "Unknown component type.");
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

    Pine::Entity* Resolve(const json& reference, const std::string& path)
    {
        Values::Object(reference, path, { "id" }, { "id" });
        const auto entity = Pine::Entities::Find(Values::Id(reference.at("id"), path + "/id"));
        Values::Require(entity != nullptr, path + "/id", "Entity does not exist.");
        Values::Require(IsSceneEntity(entity), path + "/id", "Expected a scene entity, not an editor entity.");
        return entity;
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

    struct SpatialFilter
    {
        bool UseBounds = false;
        bool Sphere = false;
        glm::dvec3 Minimum;
        glm::dvec3 Maximum;
        glm::dvec3 Center;
        double Radius = 0;

        explicit SpatialFilter(const json& value)
        {
            const std::string path = "/filter/spatial";
            Values::Object(value, path, { "test", "radius", "bounds" }, { "test" });
            UseBounds = Choice(value.at("test"), path + "/test", { "pivot", "bounds" }) == "bounds";
            Values::Require(value.contains("radius") != value.contains("bounds"), path,
                "Supply exactly one of radius or bounds.");
            Sphere = value.contains("radius");
            if (Sphere)
            {
                const auto& radius = value.at("radius");
                Values::Object(radius, path + "/radius", { "center", "distance" }, { "center", "distance" });
                Center = Vector(radius.at("center"), path + "/radius/center");
                Radius = Number(radius.at("distance"), path + "/radius/distance");
                Values::Require(Radius >= 0, path + "/radius/distance", "Expected a nonnegative radius.");
            }
            else
            {
                const auto& bounds = value.at("bounds");
                Values::Object(bounds, path + "/bounds", { "min", "max" }, { "min", "max" });
                Minimum = Vector(bounds.at("min"), path + "/bounds/min");
                Maximum = Vector(bounds.at("max"), path + "/bounds/max");
                Values::Require(glm::all(glm::lessThanEqual(Minimum, Maximum)), path + "/bounds",
                    "Expected min <= max on every axis.");
            }
        }

        bool Matches(Pine::Entity* entity, const bool includeInactive) const
        {
            Spatial::Bounds bounds;
            if (UseBounds)
            {
                Spatial::AddModelBounds(entity, bounds, includeInactive);
                Spatial::AddTerrainBounds(entity, bounds, includeInactive);
                if (bounds.Empty)
                {
                    return false;
                }
            }
            else
            {
                bounds.Include(glm::dvec3(entity->GetTransform()->GetPosition()));
            }

            if (Sphere)
            {
                const auto closest = glm::clamp(Center, bounds.Min, bounds.Max);
                const auto delta = closest - Center;
                return glm::dot(delta, delta) <= Radius * Radius;
            }
            return glm::all(glm::greaterThanEqual(bounds.Max, Minimum)) &&
                glm::all(glm::lessThanEqual(bounds.Min, Maximum));
        }
    };

    struct Filter
    {
        std::optional<Pine::ComponentType> Component;
        std::optional<std::string> Name;
        bool ExactName = true;
        Pine::Entity* Root = nullptr;
        bool Descendants = true;
        bool IncludeRoot = false;
        bool IncludeInactive = true;
        std::optional<SpatialFilter> Spatial;

        explicit Filter(const json& value)
        {
            Values::Object(value, "/filter", { "component", "name", "hierarchy", "includeInactive", "spatial" });
            IncludeInactive = Boolean(value, "includeInactive", "/filter", true);
            if (value.contains("component"))
            {
                Component = ComponentType(value.at("component"), "/filter/component");
            }
            if (value.contains("name"))
            {
                const auto& name = value.at("name");
                Values::Object(name, "/filter/name", { "value", "match" }, { "value" });
                Name = Values::String(name.at("value"), "/filter/name/value");
                if (name.contains("match"))
                {
                    ExactName = Choice(name.at("match"), "/filter/name/match", { "exact", "contains" }) == "exact";
                }
            }
            if (value.contains("hierarchy"))
            {
                const auto& hierarchy = value.at("hierarchy");
                Values::Object(hierarchy, "/filter/hierarchy", { "root", "mode", "includeRoot" }, { "root" });
                Root = Resolve(hierarchy.at("root"), "/filter/hierarchy/root");
                IncludeRoot = Boolean(hierarchy, "includeRoot", "/filter/hierarchy", false);
                if (hierarchy.contains("mode"))
                {
                    Descendants = Choice(hierarchy.at("mode"), "/filter/hierarchy/mode",
                        { "children", "descendants" }) == "descendants";
                }
            }
            if (value.contains("spatial"))
            {
                Spatial.emplace(value.at("spatial"));
            }
        }

        bool Matches(Pine::Entity* entity) const
        {
            if (!IsSceneEntity(entity) || (!IncludeInactive && !entity->GetActive()))
            {
                return false;
            }
            if (Name && (ExactName ? entity->GetName() != *Name : entity->GetName().find(*Name) == std::string::npos))
            {
                return false;
            }
            if (Root != nullptr)
            {
                bool inHierarchy = entity == Root && IncludeRoot;
                for (auto parent = entity->GetParent(); parent != nullptr; parent = parent->GetParent())
                {
                    inHierarchy = inHierarchy || parent == Root;
                    if (!Descendants || inHierarchy)
                    {
                        break;
                    }
                }
                if (!inHierarchy)
                {
                    return false;
                }
            }
            if (Component)
            {
                bool found = false;
                for (const auto component : entity->GetComponents())
                {
                    if (component->GetType() == *Component && (IncludeInactive || component->IsWorldEnabled()))
                    {
                        found = true;
                        break;
                    }
                }
                if (!found)
                {
                    return false;
                }
            }
            return !Spatial || Spatial->Matches(entity, IncludeInactive);
        }
    };

    struct Projection
    {
        bool Properties = false;
        bool WorldTransform = false;
        bool LocalTransform = false;
        std::optional<std::vector<Pine::ComponentType>> Components;

        explicit Projection(const json& value)
        {
            Values::Object(value, "/include", { "properties", "worldTransform", "localTransform", "components" });
            Properties = Boolean(value, "properties", "/include", false);
            WorldTransform = Boolean(value, "worldTransform", "/include", false);
            LocalTransform = Boolean(value, "localTransform", "/include", false);
            if (value.contains("components"))
            {
                const auto& types = value.at("components");
                Values::Require(types.is_array() && types.size() <= 32, "/include/components",
                    "Expected an array of at most 32 component type names.");
                Components.emplace();
                for (std::size_t index = 0; index < types.size(); index++)
                {
                    Components->push_back(ComponentType(types[index], "/include/components/" + std::to_string(index)));
                }
            }
        }

        json Read(Pine::Entity* entity) const
        {
            auto result = Inspection::ReadIdentity(entity);
            const auto parent = entity->GetParent();
            result["parent"] = parent == nullptr ? json(nullptr) : json(parent->GetId().ToString());
            if (Properties)
            {
                result["properties"] = Editing::ReadEntityProperties(entity);
            }
            if (WorldTransform)
            {
                result["worldTransform"] = Spatial::ReadWorldTransform(entity);
            }
            if (LocalTransform)
            {
                result["localTransform"] = Spatial::ReadLocalTransform(entity);
            }

            auto components = json::array();
            for (const auto component : entity->GetComponents())
            {
                if (Components && std::find(Components->begin(), Components->end(), component->GetType()) == Components->end())
                {
                    continue;
                }
                json detail = {
                    { "id", component->GetId().ToString() },
                    { "type", Pine::ComponentTypeToString(component->GetType()) },
                    { "active", component->GetActive() }
                };
                if (Properties)
                {
                    detail["properties"] = Editing::ReadComponentProperties(component);
                }
                components.push_back(std::move(detail));
            }
            result["components"] = std::move(components);
            return result;
        }
    };

    std::string Encode(const json& value)
    {
        // Match transport replacement of invalid UTF-8 from imported entity names.
        return value.dump(-1, ' ', false, json::error_handler_t::replace);
    }
}

nlohmann::json Editor::DebugServer::Inspection::ReadIdentity(const Pine::Entity* entity)
{
    return {
        { "id", entity->GetId().ToString() }, { "internalId", entity->GetInternalId() },
        { "name", entity->GetName() }, { "active", entity->GetActive() },
        { "static", entity->GetStatic() }, { "temporary", entity->GetTemporary() }
    };
}

Editor::DebugServer::Response Editor::DebugServer::Inspection::Query(const Request& request)
{
    try
    {
        Values::Require(request.Parameters.empty(), "", "Inspection queries do not accept query parameters.");
        Values::Require(request.Body.size() <= 16 * 1024, "", "Inspection request exceeds 16 KiB.");
        const auto depthLimit = [](int depth, json::parse_event_t, json&)
        {
            Values::Require(depth <= 8, "", "Inspection JSON nesting exceeds 8 levels.");
            return true;
        };
        const auto body = json::parse(request.Body, depthLimit, false);
        Values::Require(!body.is_discarded(), "", "Request body is not valid JSON.");
        Values::Object(body, "", { "entities", "filter", "include", "limit", "sceneGeneration" });
        const bool explicitEntities = body.contains("entities");
        Values::Require(!explicitEntities || (!body.contains("filter") && !body.contains("limit")), "",
            "Explicit entity batches cannot supply filter or limit.");

        const Projection projection(body.value("include", json::object()));
        std::size_t limit = MaximumResults;
        if (body.contains("limit"))
        {
            const auto& value = body.at("limit");
            Values::Require(value.is_number_integer() && value >= 1 && value <= MaximumResults,
                "/limit", "Expected an integer from 1 to 128.");
            limit = value.get<std::size_t>();
        }
        const auto generation = Pine::Entities::GetSceneGeneration();
        if (body.contains("sceneGeneration"))
        {
            const auto& expected = body.at("sceneGeneration");
            Values::Require(expected.is_number_unsigned() || (expected.is_number_integer() && expected >= 0),
                "/sceneGeneration", "Expected a nonnegative integer.");
            if (expected.get<std::uint64_t>() != generation)
            {
                return Error(409, "Scene was replaced. Reacquire entity references.");
            }
        }

        // Check the lifetime before resolving hierarchy roots or explicit references.
        const Filter filter(body.value("filter", json::object()));
        std::vector<Pine::Entity*> matches;
        if (explicitEntities)
        {
            const auto& references = body.at("entities");
            Values::Require(references.is_array() && !references.empty() && references.size() <= MaximumResults,
                "/entities", "Expected 1 to 128 entity references.");
            for (std::size_t index = 0; index < references.size(); index++)
            {
                matches.push_back(Resolve(references[index], "/entities/" + std::to_string(index)));
            }
        }
        else
        {
            for (const auto entity : Pine::Entities::GetList())
            {
                if (filter.Matches(entity))
                {
                    matches.push_back(entity);
                }
            }
        }

        json result = {
            { "sceneGeneration", generation }, { "entities", json::array() },
            { "total", matches.size() }, { "truncated", false }
        };
        auto& entities = result["entities"];
        std::size_t responseBytes = Encode(result).size();
        for (const auto entity : matches)
        {
            if (entities.size() == limit)
            {
                break;
            }
            auto detail = projection.Read(entity);
            const auto additionalBytes = Encode(detail).size() + (entities.empty() ? 0 : 1);
            if (responseBytes + additionalBytes > MaximumResponseBytes)
            {
                if (explicitEntities || entities.empty())
                {
                    return Error(413, "Inspection response exceeds 1 MiB. Request fewer entities or component properties.");
                }
                break;
            }
            responseBytes += additionalBytes;
            entities.push_back(std::move(detail));
        }
        result["truncated"] = entities.size() < matches.size();

        // Send the exact compact encoding we budgeted; the default transport pretty-prints JSON.
        const auto encoded = Encode(result);
        Response response;
        response.Binary.assign(encoded.begin(), encoded.end());
        return response;
    }
    catch (const Values::ValidationError& exception)
    {
        return { 400, { { "error", exception.what() }, { "path", exception.Path } } };
    }
}
