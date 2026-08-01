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
        MonoClass* GetComponentClass(ComponentType type);
        MonoClass* GetRayCastHitClass();

        ObjectHandle CreateScriptObject(const CSharpScript* script, const Component* component);
        ObjectHandle CreateEntity(std::uint32_t entityId, std::uint32_t internalId);
        ObjectHandle CreateComponent(const Component* engineComponent);
        ObjectHandle CreateAsset(const Asset* asset);

        void DisposeEntity(ObjectHandle* handle);
        void DisposeComponent(const Component* component, ObjectHandle* handle);
        void DisposeObject(ObjectHandle* handle);
    }
}
