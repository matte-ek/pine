#include "GameAssembly.hpp"

#include "Pine/Core/Log/Log.hpp"
#include "../ManagedCall/ManagedCall.hpp"

namespace
{
    constexpr auto GameAssemblyTypeName = "Pine.Core.GameAssembly, Pine";

    // Which lifecycle methods a script class has, as ResolveClass answers it. Mirrors
    // Pine.Core.GameAssembly.ScriptMethods.
    enum ScriptMethods : std::int32_t
    {
        ScriptMethod_OnStart = 1 << 0,
        ScriptMethod_OnUpdate = 1 << 1,
        ScriptMethod_OnRender = 1 << 2
    };

    // The managed side's entry points, resolved once. All of them are static, and all of them
    // answer with a failure value rather than throwing - see the class' own comment.
    struct EntryPoints
    {
        std::int32_t (*Load)(const char* path) = nullptr;
        void (*Unload)() = nullptr;

        std::int32_t (*ResolveClass)(const char* namespaceName, const char* className) = nullptr;
        std::int32_t (*GetDeclaredMethods)(std::int32_t classId) = nullptr;
        std::uint64_t (*NewClassTypeHandle)(std::int32_t classId) = nullptr;

        void (*OnStart)(std::uint64_t script, std::int32_t classId) = nullptr;
        void (*OnUpdate)(std::uint64_t script, std::int32_t classId, float deltaTime) = nullptr;
        void (*OnRender)(std::uint64_t script, std::int32_t classId, float deltaTime) = nullptr;
    };

    EntryPoints m_EntryPoints;
}

void Pine::Script::GameAssembly::Setup()
{
    m_EntryPoints = {};

    m_EntryPoints.Load = ManagedCall::Find<decltype(EntryPoints::Load)>(
        GameAssemblyTypeName, "Load");
    m_EntryPoints.Unload = ManagedCall::Find<decltype(EntryPoints::Unload)>(
        GameAssemblyTypeName, "Unload");
    m_EntryPoints.ResolveClass = ManagedCall::Find<decltype(EntryPoints::ResolveClass)>(
        GameAssemblyTypeName, "ResolveClass");
    m_EntryPoints.GetDeclaredMethods = ManagedCall::Find<decltype(EntryPoints::GetDeclaredMethods)>(
        GameAssemblyTypeName, "GetDeclaredMethods");
    m_EntryPoints.NewClassTypeHandle = ManagedCall::Find<decltype(EntryPoints::NewClassTypeHandle)>(
        GameAssemblyTypeName, "NewClassTypeHandle");
    m_EntryPoints.OnStart = ManagedCall::Find<decltype(EntryPoints::OnStart)>(
        GameAssemblyTypeName, "InvokeOnStart");
    m_EntryPoints.OnUpdate = ManagedCall::Find<decltype(EntryPoints::OnUpdate)>(
        GameAssemblyTypeName, "InvokeOnUpdate");
    m_EntryPoints.OnRender = ManagedCall::Find<decltype(EntryPoints::OnRender)>(
        GameAssemblyTypeName, "InvokeOnRender");
}

bool Pine::Script::GameAssembly::Load(const std::filesystem::path& path)
{
    if (m_EntryPoints.Load == nullptr)
    {
        return false;
    }

    return m_EntryPoints.Load(path.string().c_str()) == 1;
}

void Pine::Script::GameAssembly::Unload()
{
    if (m_EntryPoints.Unload == nullptr)
    {
        return;
    }

    m_EntryPoints.Unload();
}

Pine::Script::ResolvedScriptClass Pine::Script::GameAssembly::ResolveClass(
    const std::string& namespaceName, const std::string& className)
{
    if (m_EntryPoints.ResolveClass == nullptr)
    {
        return {};
    }

    ResolvedScriptClass resolved;

    resolved.Id = m_EntryPoints.ResolveClass(namespaceName.c_str(), className.c_str());

    if (resolved.Id < 0)
    {
        return {};
    }

    const auto methods = m_EntryPoints.GetDeclaredMethods(resolved.Id);

    resolved.HasOnStart = (methods & ScriptMethod_OnStart) != 0;
    resolved.HasOnUpdate = (methods & ScriptMethod_OnUpdate) != 0;
    resolved.HasOnRender = (methods & ScriptMethod_OnRender) != 0;

    return resolved;
}

Pine::Script::GameAssembly::ScopedClassType::ScopedClassType(const int classId)
{
    if (m_EntryPoints.NewClassTypeHandle == nullptr)
    {
        return;
    }

    m_Handle = m_EntryPoints.NewClassTypeHandle(classId);
}

Pine::Script::GameAssembly::ScopedClassType::~ScopedClassType()
{
    ObjectHandle handle = { m_Handle };

    ObjectFactory::DisposeObject(&handle);
}

std::uint64_t Pine::Script::GameAssembly::ScopedClassType::GetHandle() const
{
    return m_Handle;
}

bool Pine::Script::GameAssembly::ScopedClassType::IsValid() const
{
    return m_Handle != 0;
}

void Pine::Script::GameAssembly::OnStart(const ObjectHandle& script, const int classId)
{
    if (m_EntryPoints.OnStart == nullptr)
    {
        return;
    }

    m_EntryPoints.OnStart(script.Id, classId);
}

void Pine::Script::GameAssembly::OnUpdate(const ObjectHandle& script, const int classId,
    const float deltaTime)
{
    if (m_EntryPoints.OnUpdate == nullptr)
    {
        return;
    }

    m_EntryPoints.OnUpdate(script.Id, classId, deltaTime);
}

void Pine::Script::GameAssembly::OnRender(const ObjectHandle& script, const int classId,
    const float deltaTime)
{
    if (m_EntryPoints.OnRender == nullptr)
    {
        return;
    }

    m_EntryPoints.OnRender(script.Id, classId, deltaTime);
}
