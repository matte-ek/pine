#include "Collider.hpp"

#include "../../Values/Values.hpp"
#include "Pine/Core/Serialization/Json/SerializationJson.hpp"
#include "Pine/World/Components/Collider/Collider.hpp"

namespace
{
    using nlohmann::json;
    namespace Values = Editor::DebugServer::Editing::Values;

    const char* TypeName(const Pine::ColliderType type)
    {
        switch (type)
        {
        case Pine::ColliderType::Box: return "Box";
        case Pine::ColliderType::Sphere: return "Sphere";
        case Pine::ColliderType::Capsule: return "Capsule";
        case Pine::ColliderType::ConvexMesh: return "ConvexMesh";
        case Pine::ColliderType::ConcaveMesh: return "ConcaveMesh";
        case Pine::ColliderType::HeightField: return "HeightField";
        }
        return "Unknown";
    }

    json Read(const Pine::Component* component)
    {
        const Pine::Collider defaults;
        const auto collider = component == nullptr ? &defaults : static_cast<const Pine::Collider*>(component);
        return {
            { "Type", TypeName(collider->GetColliderType()) },
            { "Position", Pine::SerializationJson::StoreVector3(collider->GetPosition()) },
            { "Size", Pine::SerializationJson::StoreVector3(collider->GetSize()) },
            { "Layer", collider->GetLayer() },
            { "LayerMask", collider->GetLayerMask() },
            { "IsTrigger", collider->IsTrigger() },
            { "TriggerMask", collider->GetTriggerMask() }
        };
    }

    void Validate(const json& state, const std::string& path)
    {
        for (const auto axis : { "x", "y", "z" })
        {
            Values::Require(state.at("Size").at(axis).get<float>() > 0.f,
                path + "/Size/" + axis, "Size must remain positive as float32.");
        }
    }

    void Apply(Pine::Component* component, const json& state)
    {
        const auto collider = static_cast<Pine::Collider*>(component);
        const auto name = state.at("Type").get<std::string>();
        auto type = Pine::ColliderType::Box;
        if (name == "Sphere")
        {
            type = Pine::ColliderType::Sphere;
        }
        else if (name == "Capsule")
        {
            type = Pine::ColliderType::Capsule;
        }

        collider->SetColliderType(type);
        collider->SetPosition(Values::Vector3(state.at("Position")));
        collider->SetSize(Values::Vector3(state.at("Size")));
        collider->SetLayer(state.at("Layer").get<std::uint32_t>());
        collider->SetLayerMask(state.at("LayerMask").get<std::uint32_t>());
        collider->SetIsTrigger(state.at("IsTrigger").get<bool>());
        collider->SetTriggerMask(state.at("TriggerMask").get<std::uint32_t>());
    }
}

const Editor::DebugServer::Editing::Components::Adapter&
Editor::DebugServer::Editing::Components::Collider::GetAdapter()
{
    static const Adapter adapter = {
        Pine::ComponentType::Collider, "Collider",
        {
            { "Type", { { "type", "enum" }, { "values", { "Box", "Sphere", "Capsule" } },
                { "description", "3D primitive collider. Without a RigidBody it creates static collision on play." } } },
            { "Position", { { "type", "vector3" }, { "units", "world units" },
                { "space", "world-axis offset from entity world position" },
                { "description", "Offset is not rotated or scaled by the entity." } } },
            { "Size", { { "type", "vector3" }, { "minimum", 0 }, { "units", "world units before world scale" },
                { "constraint", "All coordinates must be positive as float32. Scaled dimensions must be finite and positive on play." },
                { "description", "Box: half-extents xyz. Sphere: radius x. Capsule: radius x and cylinder half-height y, aligned to Y. Unused coordinates are retained. Multiplied componentwise by entity world scale." } } },
            { "Layer", { { "type", "integer" }, { "minimum", 0 }, { "maximum", 4294967295ULL },
                { "description", "Unsigned 32-bit membership bit mask." } } },
            { "LayerMask", { { "type", "integer" }, { "minimum", 0 }, { "maximum", 4294967295ULL },
                { "description", "Unsigned 32-bit collision mask; both objects must allow the other's layer." } } },
            { "IsTrigger", { { "type", "boolean" },
                { "description", "Creates a non-solid trigger shape; does not add gameplay event callbacks." } } },
            { "TriggerMask", { { "type", "integer" }, { "minimum", 0 }, { "maximum", 4294967295ULL },
                { "description", "Unsigned 32-bit trigger mask; either trigger may allow the other's layer." } } }
        },
        Read, Validate, Apply, true
    };
    return adapter;
}
