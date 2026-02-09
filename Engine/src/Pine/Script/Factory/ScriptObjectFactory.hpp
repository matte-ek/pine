#pragma once
#include <cstdint>
#include <mono/metadata/object-forward.h>

namespace Pine
{
    enum class ComponentType;
    class Component;
    class Asset;
    class CSharpScript;
}

namespace Pine::Script
{
    struct ObjectHandle
    {
        MonoObject* Object = nullptr;
        std::uint32_t Handle = 0;
    };

    namespace ObjectFactory
    {
        void Setup();

        MonoClass* GetEntityClass();
        MonoClass* GetComponentClass(Pine::ComponentType type);
        MonoClass* GetRayCastHitClass();

        ObjectHandle CreateScriptObject(const Pine::CSharpScript* script, const Pine::Component* component);
        ObjectHandle CreateEntity(std::uint32_t entityId, std::uint32_t internalId);
        ObjectHandle CreateComponent(const Pine::Component* engineComponent);
        ObjectHandle CreateAsset(const Pine::Asset* asset);

        void DisposeEntity(ObjectHandle* handle);
        void DisposeComponent(const Component* component, ObjectHandle* handle);
        void DisposeObject(ObjectHandle* handle);
    }
}
