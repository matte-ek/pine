#pragma once
#include "Pine/World/Components/Components.hpp"
#include "Pine/World/Components/IComponent/IComponent.hpp"
#include "Pine/World/Components/Transform/Transform.hpp"
#include <cstdint>
#include <string>
#include <vector>

namespace Pine
{

    class Entity
    {
    private:
        std::uint32_t m_Id = 0;

        bool m_Active = true;

        std::string m_Name;

        std::vector<IComponent*> m_Components;
        std::vector<Entity*> m_Children;

        Entity* m_Parent = nullptr;
    public:
        explicit Entity(std::uint32_t id);
        ~Entity();

        std::uint32_t GetId() const;

        void SetActive(bool value);
        bool GetActive() const;

        void SetName(const std::string& name);
        const std::string& GetName() const;

        void SetParent(Entity* entity);
        Entity* GetParent() const;

        // Creates and attaches the component to the entity
        template <typename T>
        T* AddComponent()
        {
            auto& component = Components::Create<T>();

            component.SetParent(this);

            m_Components.push_back(&component);

            return &component;
        }

        // Creates and attaches the component to the entity
        IComponent* AddComponent(ComponentType type);

        // Attaches an existing component to the entity, please prefer to use
        // the other AddComponent()'s if you want to create a new component though.
        IComponent* AddComponent(IComponent* component);

        // Removes the first component with the specified type from the entity, returns
        // true if any component was removed.
        template <typename T>
        bool RemoveComponent()
        {
            for (auto& component : m_Components)
            {
                if (typeid(*component) == typeid(T))
                {
                    if (RemoveComponent(component))
                        return true;
                }
            }

            return false;
        }

        // Removes the component with the provided pointer, returns false on failure.
        bool RemoveComponent(IComponent* component);

        // Returns the fist component with the specified type within the entity, or
        // nullptr if none is found.
        template<typename T>
        T* GetComponent()
        {
            for (auto component : m_Components)
            {
                if (typeid(*component) == typeid(T))
                {
                    return dynamic_cast<T*>(component);
                }
            }

            return nullptr;
        }

        // Returns the transform component for the entity, will
        // never be nullptr.
        Transform* GetTransform() const;

        // Returns a list of all attached components to the entity.
        const std::vector<IComponent*>& GetComponents() const;

        // Creates a new entity and marks it as a child to this entity.
        Entity* CreateChild();

        // Add existing entity as a child to this entity.
        void AddChild(Entity* entity);

        // Unlinks the child from this entity, this will not remove the
        // entity itself from the entity list.
        void RemoveChild(Entity* entity);

        const std::vector<Entity*>& GetChildren() const;

        void Delete();

        static Entity* Create();
        static Entity* Create(const std::string& name);
    };

}