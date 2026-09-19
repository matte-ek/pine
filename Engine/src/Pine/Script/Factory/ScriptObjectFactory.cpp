#include "ScriptObjectFactory.hpp"

#include "Pine/Assets/Asset/Asset.hpp"
#include "Pine/Assets/CSharpScript/CSharpScript.hpp"
#include "Pine/Core/Log/Log.hpp"
#include "Pine/Core/UId/UId.hpp"
#include "Pine/Script/GameAssembly/GameAssembly.hpp"
#include "../ManagedCall/ManagedCall.hpp"
#include "Pine/Script/Scripts/ScriptData.hpp"
#include "Pine/World/Components/Component/Component.hpp"
#include "Pine/World/Entity/Entity.hpp"

namespace
{
    constexpr auto ObjectFactoryTypeName = "Pine.Core.ObjectFactory, Pine";

    // Pine::UId's two halves, laid out as Pine.Core.UId reads them. A UId crosses by value, so the
    // engine hands over the raw halves rather than the class that wraps them.
    struct ManagedUId
    {
        std::uint64_t Time = 0;
        std::uint64_t Random = 0;
    };

    // The managed factory's entry points, resolved once. All of them are static, and all of them
    // answer with a failure value rather than throwing - see the class' own comment.
    struct EntryPoints
    {
        std::uint64_t (*CreateEntity)(ManagedUId id, std::uint32_t internalId) = nullptr;

        std::uint64_t (*CreateComponent)(std::int32_t componentType, std::uint32_t internalId, std::uint64_t parentHandle) = nullptr;

        std::uint64_t (*CreateScriptObject)(std::uint64_t typeHandle, std::int32_t componentType, std::uint32_t internalId, std::uint64_t parentHandle) = nullptr;

        std::uint64_t (*CreateAsset)(std::int32_t assetType, ManagedUId id) = nullptr;

        void (*DisposeEntity)(std::uint64_t handle) = nullptr;

        void (*DisposeComponent)(std::uint64_t handle) = nullptr;

        void (*DisposeObject)(std::uint64_t handle) = nullptr;
    };

    EntryPoints m_EntryPoints;

    ManagedUId ToManaged(const Pine::UId& id)
    {
        return { id.GetTime(), id.GetRandom() };
    }

    // The mirror an engine object's parent entity is held by. A component whose entity has no
    // mirror has nothing to be parented to, and is not created at all.
    std::uint64_t ParentHandleOf(const Pine::Component* component)
    {
        return component->GetParent()->GetScriptHandle()->Id;
    }

    // Hand a mirror over to be let go of, and stop referring to it here. The handle is cleared
    // whatever managed code made of the call: the engine object it stood for is gone either way,
    // and keeping the value would leave the engine pointing at an object nothing owns.
    void Dispose(void (*entryPoint)(std::uint64_t), Pine::Script::ObjectHandle* handle)
    {
        if (!handle->IsValid())
        {
            return;
        }

        if (entryPoint != nullptr)
        {
            entryPoint(handle->Id);
        }

        handle->Id = 0;
    }
}

void Pine::Script::ObjectFactory::Setup()
{
    m_EntryPoints = {};

    m_EntryPoints.CreateEntity = ManagedCall::Find<decltype(EntryPoints::CreateEntity)>(
        ObjectFactoryTypeName, "CreateEntity");
    m_EntryPoints.CreateComponent = ManagedCall::Find<decltype(EntryPoints::CreateComponent)>(
        ObjectFactoryTypeName, "CreateComponent");
    m_EntryPoints.CreateScriptObject = ManagedCall::Find<decltype(EntryPoints::CreateScriptObject)>(
        ObjectFactoryTypeName, "CreateScriptObject");
    m_EntryPoints.CreateAsset = ManagedCall::Find<decltype(EntryPoints::CreateAsset)>(
        ObjectFactoryTypeName, "CreateAsset");
    m_EntryPoints.DisposeEntity = ManagedCall::Find<decltype(EntryPoints::DisposeEntity)>(
        ObjectFactoryTypeName, "DisposeEntity");
    m_EntryPoints.DisposeComponent = ManagedCall::Find<decltype(EntryPoints::DisposeComponent)>(
        ObjectFactoryTypeName, "DisposeComponent");
    m_EntryPoints.DisposeObject = ManagedCall::Find<decltype(EntryPoints::DisposeObject)>(
        ObjectFactoryTypeName, "DisposeObject");
}

Pine::Script::ObjectHandle Pine::Script::ObjectFactory::CreateEntity(const UId& id, const std::uint32_t internalId)
{
    if (m_EntryPoints.CreateEntity == nullptr)
    {
        return {};
    }

    return { m_EntryPoints.CreateEntity(ToManaged(id), internalId) };
}

Pine::Script::ObjectHandle Pine::Script::ObjectFactory::CreateComponent(const Component* engineComponent)
{
    if (m_EntryPoints.CreateComponent == nullptr)
    {
        return {};
    }

    const auto parentHandle = ParentHandleOf(engineComponent);

    if (parentHandle == 0)
    {
        return {};
    }

    return { m_EntryPoints.CreateComponent(static_cast<std::int32_t>(engineComponent->GetType()),
        engineComponent->GetInternalId(), parentHandle) };
}

Pine::Script::ObjectHandle Pine::Script::ObjectFactory::CreateScriptObject(const CSharpScript* script, const Component* component)
{
    if (m_EntryPoints.CreateScriptObject == nullptr)
    {
        return {};
    }

    const auto scriptData = script->GetScriptData();

    if (!scriptData || !scriptData->IsReady)
    {
        PWarning(fmt::format("Failed to create script object for {}, script data is not ready.",
            script->GetFilePath().string()));

        return {};
    }

    // The script's class lives in the game assembly, which the engine addresses by id rather than
    // holding anything of - see GameAssembly. It crosses as a handle to its Type, like any other
    // managed object, and the instance keeps its own class alive from there on.
    const GameAssembly::ScopedClassType classType(scriptData->ClassId);

    if (!classType.IsValid())
    {
        return {};
    }

    return { m_EntryPoints.CreateScriptObject(classType.GetHandle(),
        static_cast<std::int32_t>(component->GetType()), component->GetInternalId(),
        ParentHandleOf(component)) };
}

Pine::Script::ObjectHandle Pine::Script::ObjectFactory::CreateAsset(const Asset* asset)
{
    if (m_EntryPoints.CreateAsset == nullptr)
    {
        return {};
    }

    return { m_EntryPoints.CreateAsset(static_cast<std::int32_t>(asset->GetType()), ToManaged(asset->GetUId())) };
}

void Pine::Script::ObjectFactory::DisposeObject(ObjectHandle* handle)
{
    Dispose(m_EntryPoints.DisposeObject, handle);
}

void Pine::Script::ObjectFactory::DisposeEntity(ObjectHandle* handle)
{
    Dispose(m_EntryPoints.DisposeEntity, handle);
}

void Pine::Script::ObjectFactory::DisposeComponent(ObjectHandle* handle)
{
    Dispose(m_EntryPoints.DisposeComponent, handle);
}
