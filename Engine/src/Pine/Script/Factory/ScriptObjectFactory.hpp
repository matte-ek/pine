#pragma once
#include <cstdint>

namespace Pine
{
    enum class ComponentType;
    class Component;
    class Asset;
    class CSharpScript;
    class UId;
}

namespace Pine::Script
{
    // An engine object's managed mirror, held by the engine for as long as the object lives.
    //
    // The scripting runtime owns the object itself and is free to move or collect it, so this is
    // an opaque identity rather than a pointer - nothing, inside Script/ or out, dereferences it.
    // The engine passes the value back whenever the object itself is needed, and managed code
    // resolves it. Zero means "no object".
    struct ObjectHandle
    {
        std::uint64_t Id = 0;

        bool IsValid() const
        {
            return Id != 0;
        }
    };

    namespace ObjectFactory
    {
        void Setup();

        ObjectHandle CreateScriptObject(const CSharpScript* script, const Component* component);
        ObjectHandle CreateEntity(const UId& id, std::uint32_t internalId);
        ObjectHandle CreateComponent(const Component* engineComponent);
        ObjectHandle CreateAsset(const Asset* asset);

        void DisposeEntity(ObjectHandle* handle);
        void DisposeComponent(ObjectHandle* handle);
        void DisposeObject(ObjectHandle* handle);
    }
}
