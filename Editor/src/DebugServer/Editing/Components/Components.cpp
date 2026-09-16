#include "Components.hpp"

#include "Camera/Camera.hpp"
#include "Collider/Collider.hpp"
#include "Light/Light.hpp"
#include "ModelRenderer/ModelRenderer.hpp"
#include "RigidBody/RigidBody.hpp"
#include "Transform/Transform.hpp"
#include "../Values/Values.hpp"

namespace Editor::DebugServer::Editing::Components
{
    const std::vector<const Adapter*>& GetAdapters()
    {
        static const std::vector<const Adapter*> adapters =
        {
            &Transform::GetAdapter(), &ModelRenderer::GetAdapter(), &Light::GetAdapter(), &Camera::GetAdapter(),
            &Collider::GetAdapter(), &RigidBody::GetAdapter()
        };
        return adapters;
    }

    const Adapter* Find(const std::string& name)
    {
        for (const auto adapter : GetAdapters())
        {
            if (name == adapter->Name)
            {
                return adapter;
            }
        }
        return nullptr;
    }

    const Adapter* Find(const Pine::ComponentType type)
    {
        for (const auto adapter : GetAdapters())
        {
            if (type == adapter->Type)
            {
                return adapter;
            }
        }
        return nullptr;
    }

    nlohmann::json Prepare(const Adapter& adapter, nlohmann::json state,
        const nlohmann::json& properties, const std::string& path)
    {
        Values::Require(properties.is_object(), path, "Expected a property object.");
        // Merge only at the property level: a supplied vector replaces the entire vector.
        state.update(properties);
        Values::Properties(state, adapter.Properties, path);
        if (adapter.Validate != nullptr)
        {
            adapter.Validate(state, path);
        }
        return state;
    }

    nlohmann::json Describe(const Adapter& adapter, const Pine::Component* component)
    {
        return {
            { "id", component->GetId().ToString() },
            { "type", adapter.Name },
            { "properties", adapter.Read(component) }
        };
    }
}
