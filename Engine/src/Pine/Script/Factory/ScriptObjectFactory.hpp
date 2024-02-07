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

    ObjectHandle CreateScriptObject(Pine::CSharpScript* script, Pine::IComponent* component);
    ObjectHandle CreateEntity(std::uint32_t entityId, std::uint32_t internalId);
    ObjectHandle CreateComponent(Pine::IComponent* component);
    ObjectHandle CreateAsset(Pine::IAsset* asset);

    void DisposeObject(ObjectHandle* handle);
}