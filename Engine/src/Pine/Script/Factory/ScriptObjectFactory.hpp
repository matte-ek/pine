#pragma once
#include <mono/metadata/image.h>
#include <cstdint>
#include "Pine/Assets/CSharpScript/CSharpScript.hpp"

namespace Pine
{
    class IComponent;
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
    void DestroyScriptObject(ObjectHandle* component);

    ObjectHandle CreateEntity(std::uint32_t entityId, std::uint32_t internalId);
    void DestroyEntity(ObjectHandle* entity);

    ObjectHandle CreateComponent(Pine::IComponent* component);
    void DestroyComponent(ObjectHandle* component);
}