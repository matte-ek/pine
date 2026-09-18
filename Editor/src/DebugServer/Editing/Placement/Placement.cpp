#include "Placement.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

#include "../Values/Values.hpp"
#include "../../Spatial/Spatial.hpp"
#include "Pine/Assets/Model/Model.hpp"
#include "Pine/Core/Serialization/Json/SerializationJson.hpp"

namespace
{
    using nlohmann::json;
    namespace Values = Editor::DebugServer::Editing::Values;
    namespace Components = Editor::DebugServer::Editing::Components;
    namespace Placement = Editor::DebugServer::Editing::Placement;

    const json AxisSchema = { { "type", "enum" }, { "values", { "+X", "-X", "+Y", "-Y", "+Z", "-Z" } } };
    const json VectorSchema = { { "type", "vector3" }, { "minimum", -1e12 }, { "maximum", 1e12 } };
    const json BoundSchema = { { "type", "enum" }, { "values", { "min", "center", "max" } } };

    glm::dvec3 Vector(const json& value)
    {
        return { value.at("x").get<double>(), value.at("y").get<double>(), value.at("z").get<double>() };
    }

    glm::dvec3 UnitVector(const json& value, const std::string& path)
    {
        const auto vector = Vector(value);
        const auto length = glm::length(vector);

        Values::Require(length > 1e-12, path, "Direction must have length greater than 1e-12.");

        return vector / length;
    }

    glm::dvec3 Axis(const json& value)
    {
        const auto name = value.get<std::string>();
        glm::dvec3 axis(0.0);
        axis[name[1] - 'X'] = name[0] == '+' ? 1.0 : -1.0;
        return axis;
    }

    glm::dmat3 Basis(const glm::dvec3& axis, const glm::dvec3& up, const std::string& path)
    {
        const auto projectedUp = up - axis * glm::dot(axis, up);

        Values::Require(glm::length(projectedUp) > 1e-6, path,
            "Up direction must not be parallel to the alignment or aim direction.");

        const auto perpendicularUp = glm::normalize(projectedUp);

        return { axis, perpendicularUp, glm::cross(axis, perpendicularUp) };
    }

    void RequireFinite(const glm::dvec3& vector, const std::string& path)
    {
        for (int axis = 0; axis < 3; axis++)
        {
            Values::Require(std::isfinite(vector[axis]) && std::abs(vector[axis]) <= 1e12,
                path, "Coordinates and scales must be finite with absolute value at most 1e12.");
        }
    }

    Editor::DebugServer::Spatial::Bounds ModelBounds(
        const Editor::DebugServer::Editing::Duplication::EntityState& entity, const std::string& path,
        const Placement::WorldTransform& transform = {})
    {
        Editor::DebugServer::Spatial::Bounds bounds;

        for (const auto& component : entity.Components)
        {
            if (component.Type != Pine::ComponentType::ModelRenderer)
            {
                continue;
            }

            const auto adapter = Components::Find(component.Type);
            const auto state = Components::Prepare(*adapter, adapter->Read(nullptr), component.Properties, path);
            const auto model = static_cast<Pine::Model*>(Values::ResolvedAsset(state.at("Model")));

            if (model == nullptr || model->GetMeshes().empty())
            {
                continue;
            }

            const int meshIndex = state.at("MeshIndex").get<int>();
            const glm::dvec3 minimum = meshIndex == -1 ? model->GetBoundingBoxMin()
                : model->GetMeshes()[meshIndex]->GetBoundingBoxMin();
            const glm::dvec3 maximum = meshIndex == -1 ? model->GetBoundingBoxMax()
                : model->GetMeshes()[meshIndex]->GetBoundingBoxMax();

            RequireFinite(minimum, path);
            RequireFinite(maximum, path);
            Values::Require(glm::all(glm::lessThanEqual(minimum, maximum)), path, "Invalid model bounds.");

            // Transform each renderer's box before combining, matching /spatial/query.
            for (int corner = 0; corner < 8; corner++)
            {
                const glm::dvec3 localCorner(
                    corner & 1 ? maximum.x : minimum.x,
                    corner & 2 ? maximum.y : minimum.y,
                    corner & 4 ? maximum.z : minimum.z);
                const auto point = glm::dvec3(transform.Position) + glm::dquat(transform.Rotation)
                    * (localCorner * glm::dvec3(transform.Scale));
                RequireFinite(point, path);
                bounds.Include(point);
            }
        }

        Values::Require(!bounds.Empty, path, "Bounds placement requires assigned ModelRenderer geometry on this entity.");

        return bounds;
    }

    double BoundCoordinate(const Editor::DebugServer::Spatial::Bounds& bounds, const int axis, const json& anchor)
    {
        if (anchor == "min")
        {
            return bounds.Min[axis];
        }
        if (anchor == "max")
        {
            return bounds.Max[axis];
        }
        return (bounds.Min[axis] + bounds.Max[axis]) * 0.5;
    }
}

nlohmann::json Placement::Fields()
{
    auto point = VectorSchema;
    point["space"] = "world";
    point["units"] = "world units";

    auto direction = VectorSchema;
    direction["space"] = "world";
    direction["normalized"] = true;
    direction["constraint"] = "length > 1e-12; normalized by server";

    const json boundsAxis = {
        { "type", "object" }, { "required", { "target", "reference" } }, { "additionalFields", false },
        { "fields", { { "target", BoundSchema }, { "reference", BoundSchema } } }
    };

    return {
        { "relativeTo", {
            { "type", "reference" }, { "kind", "entity" }, { "forms", { "id" } },
            { "constraint", "Requires boundsAlignment; forbids surface, anchor, clearance and alignment. Reference cannot be target or its descendant." },
            { "description", "Existing scene entity, sampled after preceding batch operations; own model geometry only." }
        } },
        { "boundsAlignment", {
            { "type", "object" }, { "minProperties", 1 }, { "additionalFields", false },
            { "fields", { { "x", boundsAxis }, { "y", boundsAxis }, { "z", boundsAxis } } },
            { "space", "world" }, { "requires", "relativeTo" },
            { "description", "Align target and reference world-axis bounds on selected axes; omitted axes retain position. Preserves rotation and scale. Includes own model boxes and MeshIndex, excludes descendants and non-model geometry." }
        } },
        { "offset", {
            { "type", "object" }, { "required", { "space", "value" } }, { "additionalFields", false },
            { "requires", "relativeTo" }, { "whenOmitted", "zero" },
            { "fields", {
                { "space", { { "type", "enum" }, { "values", { "world", "referenceLocal" } } } },
                { "value", VectorSchema }
            } },
            { "units", "world units" },
            { "description", "Translation added after bounds alignment, on all axes. referenceLocal rotates by reference world rotation, ignoring scale and mirroring." }
        } },
        { "surface", {
            { "type", "object" }, { "required", { "point", "normal" } }, { "additionalFields", false },
            { "fields", { { "point", point }, { "normal", direction } } },
            { "description", "Supplied plane; normal points toward the prop. No surface lookup or collision test." }
        } },
        { "anchor", {
            { "type", "object" }, { "required", { "type" } }, { "additionalFields", false },
            { "fields", {
                { "type", { { "type", "enum" }, { "values", { "modelBounds", "localPoint" } } } },
                { "point", { { "type", "vector3" }, { "space", "model local before scale" },
                    { "units", "world units" }, { "minimum", -1e12 }, { "maximum", 1e12 } } }
            } },
            { "constraint", "localPoint requires point; modelBounds forbids point and requires own model geometry" },
            { "description", "modelBounds centers the transformed local box over the surface point and puts its minimum normal projection at clearance; excludes descendants and non-model geometry." }
        } },
        { "clearance", { { "type", "number" }, { "minimum", 0 }, { "maximum", 1e12 },
            { "default", 0 }, { "units", "world units along normalized surface normal" } } },
        { "alignment", {
            { "type", "object" }, { "required", { "axis", "upAxis", "up" } }, { "additionalFields", false },
            { "whenOmitted", "preserve world rotation" },
            { "fields", { { "axis", AxisSchema }, { "upAxis", AxisSchema }, { "up", direction } } },
            { "constraint", "axis and upAxis must be perpendicular; up must not be parallel to surface normal" },
            { "description", "Rotate local axis to normal and local upAxis toward projected world up. Axes ignore scale and mirroring." }
        } }
    };
}

void Placement::Validate(json& input, const std::string& path)
{
    Values::Require(input.contains("surface") != input.contains("relativeTo"), path,
        "Supply exactly one of surface or relativeTo.");

    if (input.contains("relativeTo"))
    {
        Values::Object(input, path, { "relativeTo", "boundsAlignment", "offset" }, { "relativeTo", "boundsAlignment" });
        Values::Object(input.at("relativeTo"), path + "/relativeTo", { "id" }, { "id" });
        Values::Id(input.at("relativeTo").at("id"), path + "/relativeTo/id");

        auto& axes = input.at("boundsAlignment");
        Values::Object(axes, path + "/boundsAlignment", { "x", "y", "z" });
        Values::Require(!axes.empty(), path + "/boundsAlignment", "Select at least one world axis.");

        for (auto& axis : axes.items())
        {
            const auto axisPath = path + "/boundsAlignment/" + axis.key();
            Values::Object(axis.value(), axisPath, { "target", "reference" }, { "target", "reference" });
            Values::Properties(axis.value(), { { "target", BoundSchema }, { "reference", BoundSchema } }, axisPath);
        }

        if (input.contains("offset"))
        {
            auto& offset = input.at("offset");
            Values::Object(offset, path + "/offset", { "space", "value" }, { "space", "value" });
            Values::Properties(offset, Fields().at("offset").at("fields"), path + "/offset");
        }

        return;
    }

    Values::Object(input, path, { "surface", "anchor", "clearance", "alignment" }, { "surface", "anchor" });

    auto& surface = input.at("surface");
    Values::Object(surface, path + "/surface", { "point", "normal" }, { "point", "normal" });
    Values::Properties(surface, { { "point", VectorSchema }, { "normal", VectorSchema } }, path + "/surface");
    const auto normal = UnitVector(surface.at("normal"), path + "/surface/normal");

    auto& anchor = input.at("anchor");
    Values::Object(anchor, path + "/anchor", { "type", "point" }, { "type" });
    Values::Properties(anchor, Fields().at("anchor").at("fields"), path + "/anchor");

    const bool localPoint = anchor.at("type") == "localPoint";
    Values::Require(anchor.contains("point") == localPoint, path + "/anchor/point",
        "point is required for localPoint and forbidden for modelBounds.");

    json clearance = { { "clearance", input.value("clearance", json(0)) } };
    Values::Properties(clearance, { { "clearance", Fields().at("clearance") } }, path);
    input["clearance"] = clearance.at("clearance");

    if (input.contains("alignment"))
    {
        auto& alignment = input.at("alignment");
        Values::Object(alignment, path + "/alignment", { "axis", "upAxis", "up" }, { "axis", "upAxis", "up" });
        Values::Properties(alignment, {
            { "axis", AxisSchema }, { "upAxis", AxisSchema }, { "up", VectorSchema }
        }, path + "/alignment");

        Basis(Axis(alignment.at("axis")), Axis(alignment.at("upAxis")), path + "/alignment/upAxis");
        Basis(normal, UnitVector(alignment.at("up"), path + "/alignment/up"), path + "/alignment/up");
    }
}

Placement::WorldTransform Placement::Compose(const WorldTransform& parent, const json& local, const std::string& path)
{
    auto validated = local;
    Values::Properties(validated, Components::Find(Pine::ComponentType::Transform)->Properties, path);

    const auto rotation = Values::Quaternion(local.at("LocalRotation"));
    Values::Require(std::abs(glm::length(glm::dquat(rotation)) - 1.0) < 1e-5, path + "/LocalRotation",
        "Placement and aiming require unit transform rotations (length tolerance 1e-5).");

    WorldTransform world;

    // These are Pine's public GetPosition/GetRotation/GetScale semantics, not matrix parenting.
    // Validation normalizes quaternions, but untouched ancestors must retain their actual values.
    world.Position = parent.Position + Values::Vector3(local.at("LocalPosition"));
    world.Rotation = parent.Rotation * rotation;
    world.Scale = parent.Scale * Values::Vector3(local.at("LocalScale"));

    RequireFinite(world.Position, path);
    RequireFinite(world.Scale, path);

    return world;
}

nlohmann::json Placement::AimFields()
{
    auto point = VectorSchema;
    point["space"] = "world";
    point["units"] = "world units";
    point["constraint"] = "Distance from the entity's world position must exceed 1e-12.";

    auto forwardAxis = AxisSchema;
    forwardAxis["description"] = "Local rotation axis to aim at point; -Z for Pine spotlights and cameras. Ignores scale and mirroring.";

    auto upAxis = AxisSchema;
    upAxis["constraint"] = "Must be perpendicular to forwardAxis.";

    auto up = VectorSchema;
    up["space"] = "world";
    up["normalized"] = true;
    up["constraint"] = "Length > 1e-12; normalized by server. Projection perpendicular to aim direction must have length > 1e-6.";
    up["description"] = "Local upAxis follows projected world up, fixing roll. Position and scale are preserved.";

    return { { "point", point }, { "forwardAxis", forwardAxis }, { "upAxis", upAxis }, { "up", up } };
}

void Placement::ValidateAim(json& input, const std::string& path)
{
    Values::Object(input, path, { "point", "forwardAxis", "upAxis", "up" },
        { "point", "forwardAxis", "upAxis", "up" });
    Values::Properties(input, AimFields(), path);

    Basis(Axis(input.at("forwardAxis")), Axis(input.at("upAxis")), path + "/upAxis");
    UnitVector(input.at("up"), path + "/up");
}

nlohmann::json Placement::PrepareAim(const json& input, const Duplication::EntityState& entity,
    const WorldTransform& parent, const std::string& path)
{
    const auto adapter = Components::Find(Pine::ComponentType::Transform);
    auto local = Components::Prepare(*adapter, adapter->Read(nullptr), entity.Components.front().Properties, path + "/target");
    const auto world = Compose(parent, local, path + "/target");

    const auto direction = Vector(input.at("point")) - glm::dvec3(world.Position);
    const auto distance = glm::length(direction);
    Values::Require(distance > 1e-12, path + "/point",
        "Aim point must be farther than 1e-12 from the entity's world position.");

    const auto localBasis = Basis(Axis(input.at("forwardAxis")), Axis(input.at("upAxis")), path + "/upAxis");
    const auto worldBasis = Basis(direction / distance, UnitVector(input.at("up"), path + "/up"), path + "/up");
    const auto rotation = glm::normalize(glm::quat_cast(worldBasis * glm::transpose(localBasis)));
    const Pine::Quaternion localRotation = glm::normalize(glm::inverse(glm::dquat(parent.Rotation)) * rotation);
    local["LocalRotation"] = Pine::SerializationJson::StoreQuaternion(localRotation);

    return Components::Prepare(*adapter, adapter->Read(nullptr), local, path + "/target");
}

nlohmann::json Placement::Prepare(const json& input, const Duplication::EntityState& entity,
    const WorldTransform& parent, const std::string& path)
{
    const auto adapter = Components::Find(Pine::ComponentType::Transform);
    auto local = Components::Prepare(*adapter, adapter->Read(nullptr), entity.Components.front().Properties, path + "/target");
    const auto world = Compose(parent, local, path + "/target");

    const auto normal = UnitVector(input.at("surface").at("normal"), path + "/surface/normal");
    const auto point = Vector(input.at("surface").at("point"));
    glm::dquat rotation = world.Rotation;

    if (input.contains("alignment"))
    {
        const auto& alignment = input.at("alignment");
        const auto localBasis = Basis(Axis(alignment.at("axis")), Axis(alignment.at("upAxis")), path + "/alignment/upAxis");
        const auto worldBasis = Basis(normal, UnitVector(alignment.at("up"), path + "/alignment/up"), path + "/alignment/up");

        rotation = glm::normalize(glm::quat_cast(worldBasis * glm::transpose(localBasis)));
        const Pine::Quaternion localRotation = glm::normalize(glm::inverse(glm::dquat(parent.Rotation)) * rotation);
        local["LocalRotation"] = Pine::SerializationJson::StoreQuaternion(localRotation);

        // Use the rotation that the float32 engine will actually apply when computing contact.
        rotation = parent.Rotation * localRotation;
    }

    glm::dvec3 contactOffset;

    if (input.at("anchor").at("type") == "localPoint")
    {
        contactOffset = rotation * (Vector(input.at("anchor").at("point")) * glm::dvec3(world.Scale));
    }
    else
    {
        const auto bounds = ModelBounds(entity, path + "/anchor");
        const auto center = (bounds.Min + bounds.Max) * 0.5;
        const auto centerOffset = rotation * (center * glm::dvec3(world.Scale));
        double minimumProjection = std::numeric_limits<double>::max();

        for (int corner = 0; corner < 8; corner++)
        {
            const glm::dvec3 localCorner(
                corner & 1 ? bounds.Max.x : bounds.Min.x,
                corner & 2 ? bounds.Max.y : bounds.Min.y,
                corner & 4 ? bounds.Max.z : bounds.Min.z);
            const auto offset = rotation * (localCorner * glm::dvec3(world.Scale));
            minimumProjection = std::min(minimumProjection, glm::dot(offset, normal));
        }

        contactOffset = centerOffset + normal * (minimumProjection - glm::dot(centerOffset, normal));
    }

    const auto position = point + normal * input.at("clearance").get<double>() - contactOffset;
    const auto localPosition = position - glm::dvec3(parent.Position);

    RequireFinite(position, path + "/surface/point");
    RequireFinite(localPosition, path + "/target");
    local["LocalPosition"] = Pine::SerializationJson::StoreVector3(Pine::Vector3f(localPosition));

    return Components::Prepare(*adapter, adapter->Read(nullptr), local, path + "/target");
}

nlohmann::json Placement::PrepareRelative(const json& input, const Duplication::EntityState& entity,
    const WorldTransform& parent, const Duplication::EntityState& reference,
    const WorldTransform& referenceParent, const std::string& path)
{
    const auto adapter = Components::Find(Pine::ComponentType::Transform);
    auto local = Components::Prepare(*adapter, adapter->Read(nullptr), entity.Components.front().Properties, path + "/target");

    const auto world = Compose(parent, local, path + "/target");
    const auto referenceWorld = Compose(referenceParent, reference.Components.front().Properties, path + "/relativeTo");

    const auto bounds = ModelBounds(entity, path + "/target", world);
    const auto referenceBounds = ModelBounds(reference, path + "/relativeTo", referenceWorld);

    glm::dvec3 position = world.Position;

    for (const auto& alignment : input.at("boundsAlignment").items())
    {
        const int axis = alignment.key()[0] - 'x';
        position[axis] += BoundCoordinate(referenceBounds, axis, alignment.value().at("reference"))
            - BoundCoordinate(bounds, axis, alignment.value().at("target"));
    }

    if (input.contains("offset"))
    {
        const auto& offset = input.at("offset");
        auto translation = Vector(offset.at("value"));

        if (offset.at("space") == "referenceLocal")
        {
            translation = glm::dquat(referenceWorld.Rotation) * translation;
        }

        position += translation;
    }

    const auto localPosition = position - glm::dvec3(parent.Position);

    RequireFinite(position, path + "/target");
    RequireFinite(localPosition, path + "/target");
    local["LocalPosition"] = Pine::SerializationJson::StoreVector3(Pine::Vector3f(localPosition));

    return Components::Prepare(*adapter, adapter->Read(nullptr), local, path + "/target");
}
