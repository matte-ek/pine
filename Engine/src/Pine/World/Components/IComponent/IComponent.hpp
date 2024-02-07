#pragma once

#include <nlohmann/json.hpp>
#include "Pine/Script/Factory/ScriptObjectFactory.hpp"

namespace Pine
{

    // Notice: This order needs to match within Pine::Components::Setup()
    enum class ComponentType : int
    {
        Transform,
        ModelRenderer,
        TerrainRenderer,
        Camera,
        Light,
        Collider,
        RigidBody,
        Collider2D,
        RigidBody2D,
        SpriteRenderer,
        TilemapRenderer,
        NativeScript,
        Script
    };

    inline const char *ComponentTypeToString(ComponentType type)
    {
        switch (type)
        {
            case ComponentType::Transform:
                return "Transform";
            case ComponentType::ModelRenderer:
                return "Model Renderer";
            case ComponentType::TerrainRenderer:
                return "Terrain Renderer";
            case ComponentType::Camera:
                return "Camera";
            case ComponentType::Light:
                return "Light";
            case ComponentType::Collider:
                return "Collider";
            case ComponentType::RigidBody:
                return "Rigid Body";
            case ComponentType::Collider2D:
                return "Collider 2D";
            case ComponentType::RigidBody2D:
                return "Rigid Body 2D";
            case ComponentType::SpriteRenderer:
                return "Sprite Renderer";
            case ComponentType::TilemapRenderer:
                return "Tile-map Renderer";
            case ComponentType::NativeScript:
                return "Native Script";
            case ComponentType::Script:
                return "Script";
        }

        return "N/A";
    }

    class Entity;

    class IComponent
    {
    protected:
        bool m_Active = true;

        // If this component is part of the ECF or is just an object in memory.
        bool m_Standalone = true;

        Script::ObjectHandle m_ScriptObjectHandle = { nullptr, 0 };

        ComponentType m_Type = ComponentType::Transform;

        std::uint32_t m_InternalId = 0;

        Entity *m_Parent = nullptr;

        void CreateScriptInstance();
        void DestroyScriptInstance();
    public:
        explicit IComponent(ComponentType type);

        virtual ~IComponent() = default;

        void SetInternalId(std::uint32_t id);
        std::uint32_t GetInternalId() const;

        void SetActive(bool value);
        bool GetActive() const;

        void SetStandalone(bool value);
        bool GetStandalone() const;

        void SetParent(Entity *entity);
        Entity *GetParent() const;

        bool IsWorldEnabled() const;

        ComponentType GetType() const;

        Script::ObjectHandle* GetComponentScriptHandle();

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

        // Called before/after any physics updates
        virtual void OnPrePhysicsUpdate();
        virtual void OnPostPhysicsUpdate();

        virtual void LoadData(const nlohmann::json &j);
        virtual void SaveData(nlohmann::json &j);
    };

}