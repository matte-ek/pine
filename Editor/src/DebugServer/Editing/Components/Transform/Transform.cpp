#include "Transform.hpp"

#include "../../Values/Values.hpp"
#include "Pine/Core/Serialization/Json/SerializationJson.hpp"
#include "Pine/World/Entity/Entity.hpp"

namespace
{
    using nlohmann::json;
    namespace Values = Editor::DebugServer::Editing::Values;

    json Read(const Pine::Component* component)
    {
        const Pine::Transform defaults;
        const auto transform = component == nullptr ? &defaults : static_cast<const Pine::Transform*>(component);
        return {
            { "LocalPosition", Pine::SerializationJson::StoreVector3(transform->GetLocalPosition()) },
            { "LocalRotation", Pine::SerializationJson::StoreQuaternion(transform->GetLocalRotation()) },
            { "LocalScale", Pine::SerializationJson::StoreVector3(transform->GetLocalScale()) }
        };
    }

    void Apply(Pine::Component* component, const json& state)
    {
        const auto transform = static_cast<Pine::Transform*>(component);
        transform->SetLocalPosition(Values::Vector3(state.at("LocalPosition")));
        transform->SetLocalRotation(Values::Quaternion(state.at("LocalRotation")));
        transform->SetLocalScale(Values::Vector3(state.at("LocalScale")));

        // The setters reach the descendants' transforms, but not their entities' dirty flags.
        Editor::DebugServer::Editing::Components::Transform::MarkHierarchyDirty(transform->GetParent());
    }
}

void Editor::DebugServer::Editing::Components::Transform::MarkHierarchyDirty(Pine::Entity* entity)
{
    entity->SetDirty(true);
    for (const auto child : entity->GetChildren())
    {
        MarkHierarchyDirty(child);
    }
}

const Editor::DebugServer::Editing::Components::Adapter&
Editor::DebugServer::Editing::Components::Transform::GetAdapter()
{
    static const Adapter adapter = {
        Pine::ComponentType::Transform, "Transform",
        {
            { "LocalPosition", { { "type", "vector3" }, { "space", "local" }, { "units", "world units" } } },
            { "LocalRotation", { { "type", "quaternion" }, { "space", "local" }, { "normalized", true } } },
            { "LocalScale", { { "type", "vector3" }, { "space", "local" } } }
        },
        Read, nullptr, Apply
    };
    return adapter;
}
