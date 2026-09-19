#include "ScriptManager.hpp"
#include "Pine/Script/GameAssembly/GameAssembly.hpp"
#include "Pine/Core/Log/Log.hpp"
#include "Pine/Script/Scripts/ScriptData.hpp"
#include "Pine/Script/Scripts/ScriptField.hpp"
#include "Pine/Script/Scripts/ScriptFieldRegistry.hpp"
#include "Pine/Assets/CSharpScript/CSharpScript.hpp"
#include "Pine/Assets/Assets.hpp"
#include "Pine/Script/Factory/ScriptObjectFactory.hpp"
#include "Pine/World/Components/Components.hpp"
#include "Pine/World/Components/Script/ScriptComponent.hpp"
#include "Pine/World/Entity/Entity.hpp"

#include <cassert>
#include <vector>

#include "Pine/Performance/Performance.hpp"

namespace
{
    bool m_HasGameAssembly = false;

    std::filesystem::path m_GameAssemblyPath;

    std::vector<Pine::ScriptData*> m_ScriptData;

    // Returns all script assets currently loaded in the asset manager
    std::vector<Pine::CSharpScript*> GetAllScripts()
    {
        std::vector<Pine::CSharpScript*> scripts;

        for (const auto& [id, asset] : Pine::Assets::GetAll())
        {
            if (asset->GetType() != Pine::AssetType::CSharpScript)
            {
                continue;
            }

            if (asset->IsPendingDelete())
            {
                continue;
            }

            scripts.push_back(dynamic_cast<Pine::CSharpScript*>(asset));
        }

        return scripts;
    }

    // Asks the scripting runtime which of a script class' fields the editor shows, and what it
    // should know about each of them. Both the decision and the reflection itself live in Pine.dll
    // - see Pine.Core.Reflection.FieldRegistry - because they are made from custom attributes.
    void ProcessScriptFields(Pine::ScriptData* scriptData)
    {
        const Pine::Script::GameAssembly::ScopedClassType classType(scriptData->ClassId);

        // The field registry keeps its own ids, which are not the game assembly's. Nothing needs
        // to hold this one: it goes into each ScriptField as it is made, and the class is only
        // ever reached through those from here on.
        const auto fieldClassId = Pine::Script::FieldRegistry::Register(classType.GetHandle());

        if (fieldClassId < 0)
        {
            PWarning(fmt::format("Failed to reflect the fields of script: {}",
                scriptData->Asset->GetFilePath().string()));

            return;
        }

        const auto descriptors = Pine::Script::FieldRegistry::GetDescriptors(fieldClassId);

        for (std::size_t index = 0; index < descriptors.size(); index++)
        {
            scriptData->Fields.push_back(
                new Pine::ScriptField(scriptData, fieldClassId, static_cast<int>(index), descriptors[index]));
        }
    }

    // Populates all fields of a script data instance
    void ResolveScriptData(Pine::ScriptData* scriptData)
    {
        if (!m_HasGameAssembly)
        {
            return;
        }

        // The managed type is stored on the asset as a fully-qualified name (e.g. "Game.Player").
        // Fall back to the legacy convention (namespace "Game", class == file stem) for older
        // assets that predate the stored type name.
        std::string namespaceName = "Game";
        std::string className = scriptData->Asset->GetTypeName();

        if (className.empty())
        {
            className = scriptData->Asset->GetFilePath().stem().string();
        }
        else if (const auto dot = className.find_last_of('.'); dot != std::string::npos)
        {
            namespaceName = className.substr(0, dot);
            className = className.substr(dot + 1);
        }

        // The class has to exist in the game assembly and inherit from the `Script` class - that
        // is what gives an instance the Component identity the object factory writes into it. The
        // lifecycle methods are all optional, so the registry reports which ones are there rather
        // than requiring any of them.
        const auto resolved = Pine::Script::GameAssembly::ResolveClass(namespaceName, className);

        if (resolved.Id < 0)
        {
            PWarning(fmt::format("Failed to find class for script: {}.{}", namespaceName, className));

            return;
        }

        scriptData->ClassId = resolved.Id;
        scriptData->HasOnStart = resolved.HasOnStart;
        scriptData->HasOnUpdate = resolved.HasOnUpdate;
        scriptData->IsReady = true;

        ProcessScriptFields(scriptData);
    }

    // Everything the engine holds that refers into the game assembly: the resolved classes, and
    // the reflected fields Pine.dll is keeping for them. Both have to be let go of before that
    // assembly can be unloaded, which is why this is its own step rather than the first half of
    // ReloadScripts - a reload runs it before the unload, and the rebuild after.
    //
    // ReloadScripts still starts with it, because Pine::Engine::Run calls that on its own with no
    // unload anywhere near it. On the reload path it therefore runs twice, the second time over
    // nothing.
    void DestroyScriptData()
    {
        for (const auto& script : m_ScriptData)
        {
            script->Asset->SetScriptData(nullptr);

            delete script;
        }

        m_ScriptData.clear();

        Pine::Script::FieldRegistry::Reset();
    }

    // What a script component has to have before one of its lifecycle methods can be dispatched:
    // a script asset, a class resolved out of the game assembly, and a managed instance to call
    // the method on. Null when any of them is missing, in which case there is nothing to run.
    Pine::ScriptData* DispatchableScript(Pine::ScriptComponent& scriptComponent)
    {
        const auto script = scriptComponent.GetScript();

        if (!script)
        {
            return nullptr;
        }

        const auto scriptData = script->GetScriptData();

        if (!scriptData || !scriptData->IsReady)
        {
            return nullptr;
        }

        if (!scriptComponent.GetScriptObjectHandle()->IsValid())
        {
            return nullptr;
        }

        return scriptData;
    }
}

void Pine::Script::Manager::Setup()
{
    // The game assembly is loaded per-project by the host application (the Editor, once it
    // knows which project is open; the GameHost, from its baked output) after engine setup,
    // via LoadGameAssembly(). Nothing to do here at boot time.
}

void Pine::Script::Manager::Dispose()
{
}

bool Pine::Script::Manager::HasGameAssembly()
{
    return m_HasGameAssembly;
}

void Pine::Script::Manager::LoadGameAssembly(const std::filesystem::path &path)
{
    if (!GameAssembly::Load(path))
    {
        PError("Failed to load game assembly.");
        return;
    }

    m_GameAssemblyPath = path;
    m_HasGameAssembly = true;
}

void Pine::Script::Manager::ReloadGameAssembly()
{
    assert(m_HasGameAssembly);

    // Unloading the game assembly only works once nothing refers into it any more, and a single
    // reference left behind makes it fail silently - the old assembly, and everything it brought
    // with it, stays in memory for the rest of the session. So the engine lets go of all three
    // kinds of reference it holds, in order, before asking for the unload.
    for (auto& scriptComponent : Components::Get<ScriptComponent>(true))
    {
        // A script's field values live nowhere but its instance, so read them back into the
        // component first. The instance rebuilt against the new assembly starts from what the
        // author set rather than from the C# field initializers.
        scriptComponent.CaptureFieldValues();
        scriptComponent.DestroyInstance();
    }

    DestroyScriptData();

    GameAssembly::Unload();

    LoadGameAssembly(m_GameAssemblyPath);

    ReloadScripts();

    for (auto& scriptComponent : Components::Get<ScriptComponent>(true))
    {
        scriptComponent.CreateInstance();
    }
}

void Pine::Script::Manager::ReloadScripts()
{
    DestroyScriptData();

    const auto& scripts = GetAllScripts();

    for (const auto& script : scripts)
    {
        auto scriptData = new ScriptData;

        scriptData->Asset = script;

        ResolveScriptData(scriptData);

        m_ScriptData.push_back(scriptData);

        script->SetScriptData(scriptData);
    }
}

// Whatever a script throws is caught, logged with its stack trace and swallowed by managed code
// - nothing may be thrown back across the boundary - so neither loop below has anything to
// report.
void Pine::Script::Manager::OnStart()
{
    PINE_PF_SCOPE();

    for (auto& scriptComponent : Components::Get<ScriptComponent>())
    {
        const auto scriptData = DispatchableScript(scriptComponent);

        if (!scriptData || !scriptData->HasOnStart)
        {
            continue;
        }

        GameAssembly::OnStart(*scriptComponent.GetScriptObjectHandle(), scriptData->ClassId);
    }
}

void Pine::Script::Manager::OnUpdate(float deltaTime)
{
    PINE_PF_SCOPE();

    for (auto& scriptComponent : Components::Get<ScriptComponent>())
    {
        const auto scriptData = DispatchableScript(scriptComponent);

        if (!scriptData || !scriptData->HasOnUpdate)
        {
            continue;
        }

        GameAssembly::OnUpdate(*scriptComponent.GetScriptObjectHandle(), scriptData->ClassId, deltaTime);
    }
}

void Pine::Script::Manager::OnRender(float deltaTime)
{
}
