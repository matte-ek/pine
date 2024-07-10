#pragma once
#include <mono/metadata/image.h>
#include <cstdint>

namespace Pine
{
    class IComponent;
    class IAsset;
    class CSharpScript;
}

namespace Pine::Script
{
    struct ObjectHandle
    {
        MonoObject* Object = nullptr;
        std::uint32_t Handle = 0;
    };
}

namespace Pine::Script::ObjectFactory
{
    void Setup();

    ObjectHandle CreateScriptObject(const Pine::CSharpScript* script, const Pine::IComponent* component);
    ObjectHandle CreateEntity(std::uint32_t entityId, std::uint32_t internalId);
    ObjectHandle CreateComponent(const Pine::IComponent* engineComponent);
    ObjectHandle CreateAsset(const Pine::IAsset* asset);

    void DisposeEntity(ObjectHandle* handle);
    void DisposeComponent(const IComponent* component, ObjectHandle* handle);
    void DisposeObject(ObjectHandle* handle);
}