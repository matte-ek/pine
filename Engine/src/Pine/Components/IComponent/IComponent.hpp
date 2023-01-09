#pragma once
#include <nlohmann/json.hpp>

namespace Pine
{

    enum class ComponentType
    {
        Invalid,
        Transform
    };

    class Entity;

    class IComponent
    {
    protected:
        bool m_Active = true;

        // If this component is part of the ECS or is just an object in memory.
        bool m_Standalone = false;

        ComponentType m_Type = ComponentType::Invalid;

        Entity* m_Parent = nullptr;
    public:
        IComponent(Entity* parent, ComponentType type);
        virtual ~IComponent() = default;

        void SetActive(bool value);
        bool GetActive() const;

        void SetStandalone(bool value);
        bool GetStandalone() const;

        ComponentType GetType() const;

        virtual void OnCreated();
        virtual void OnDestroyed();

        // Whenever this component's memory is copied to a new component.
        // Used to over fix raw pointers to new objects.
        virtual void OnCopied();

        // Called when the game world is set to be initialized.
        // May or may not be directly after OnCreated()
        virtual void OnSetup();

        // Called on each game tick by the update thread
        virtual void OnUpdate(float deltaTime);

        // Called each frame right before rendering
        virtual void OnRender(float deltaTime);

        virtual void LoadData(const nlohmann::json& j);
        virtual void SaveData(nlohmann::json& j);
    };

}