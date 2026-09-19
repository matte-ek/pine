#include "ScriptManager.hpp"
#include "Pine/Script/Runtime/ScriptingRuntime.hpp"
#include "Pine/Core/Log/Log.hpp"
#include "Pine/Script/Scripts/ScriptData.hpp"
#include "Pine/Script/Scripts/ScriptField.hpp"
#include "Pine/Assets/CSharpScript/CSharpScript.hpp"
#include "Pine/Assets/Assets.hpp"
#include "Pine/Script/Factory/ScriptObjectFactory.hpp"
#include "Pine/World/Components/Components.hpp"
#include "Pine/World/Components/Script/ScriptComponent.hpp"
#include "Pine/World/Entity/Entity.hpp"
#include "mono/metadata/class.h"

#include <cstring>
#include <vector>

#include <mono/metadata/appdomain.h>
#include <mono/metadata/attrdefs.h>
#include <mono/metadata/exception.h>

#include "Pine/Performance/Performance.hpp"

namespace
{
    bool m_HasGameAssembly = false;

    Pine::Script::RuntimeAssembly* m_GameAssembly;
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

    // Finds all and populates all public fields for a script class
    void ProcessScriptFields(Pine::ScriptData* scriptData)
    {
        MonoClassField* field;
        void* iterator = nullptr;
        while ((field = mono_class_get_fields(scriptData->Class, &iterator)))
        {
            const auto name = mono_field_get_name(field);

            // Ignore Pine fields
            if (strcmp(name, "Parent") == 0 || strcmp(name, "Type") == 0)
                continue;

            const auto accessFlag = mono_field_get_flags(field) & MONO_FIELD_ATTR_FIELD_ACCESS_MASK;

            if (!(accessFlag & MONO_FIELD_ATTR_PUBLIC))
                continue;

            const auto type = mono_field_get_type(field);

            auto scriptField = new Pine::ScriptField(name, field, scriptData, type);

            // A type the editor can neither show nor store. The field still works in C#; it just
            // isn't reflected, so nothing downstream has to keep checking for it.
            if (scriptField->GetType() == Pine::ScriptFieldType::Invalid)
            {
                delete scriptField;

                continue;
            }

            scriptData->Fields.push_back(scriptField);
        }
    }

    // Populates all fields of a script data instance
    void ResolveScriptData(Pine::ScriptData* scriptData)
    {
        if (!m_GameAssembly)
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

        auto monoClass = mono_class_from_name(m_GameAssembly->Image, namespaceName.c_str(), className.c_str());
        if (!monoClass)
        {
            PWarning(fmt::format("Failed to find class for script: {}.{}", namespaceName, className));
            return;
        }

        scriptData->Class = monoClass;

        scriptData->MethodOnStart = mono_class_get_method_from_name(scriptData->Class, "OnStart", 0);
        scriptData->MethodOnDestroy = mono_class_get_method_from_name(scriptData->Class, "OnDestroy", 0);
        scriptData->MethodOnUpdate = mono_class_get_method_from_name(scriptData->Class, "OnUpdate", 1);
        scriptData->MethodOnRender = mono_class_get_method_from_name(scriptData->Class, "OnRender", 1);
        scriptData->ComponentParentField = mono_class_get_field_from_name(scriptData->Class, "Parent");
        scriptData->ComponentTypeField = mono_class_get_field_from_name(scriptData->Class, "Type");     
        scriptData->ComponentInternalIdField = mono_class_get_field_from_name(scriptData->Class, "_internalId");

        ProcessScriptFields(scriptData);

        // Realistically, the only requirement is that the script is inheriting from the `Script` class.
        scriptData->IsReady = scriptData->ComponentParentField && scriptData->ComponentTypeField;
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
    m_GameAssembly = Runtime::LoadAssembly(path);

    if (!m_GameAssembly)
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

    Runtime::Reset();

    LoadGameAssembly(m_GameAssemblyPath);
    ReloadScripts();

    for (auto& scriptComponent : Components::Get<ScriptComponent>(true))
    {
        scriptComponent.CreateInstance();
    }
}

void Pine::Script::Manager::ReloadScripts()
{
    for (const auto& script : m_ScriptData)
    {
        for (const auto field : script->Fields)
        {
            delete field;
        }

        script->Asset->SetScriptData(nullptr);

        delete script;
    }

    m_ScriptData.clear();

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

void Pine::Script::Manager::OnStart()
{
    PINE_PF_SCOPE();

    for (auto& scriptComponent : Components::Get<ScriptComponent>())
    {
        auto script = scriptComponent.GetScript();

        if (!script)
        {
            continue;
        }

        auto scriptData = script->GetScriptData();

        if (!scriptData || !scriptData->IsReady || !scriptData->MethodOnStart)
        {
            continue;
        }

        auto objectHandle = scriptComponent.GetScriptObjectHandle();
        if (!objectHandle || objectHandle->Object == nullptr)
        {
            continue;
        }

        MonoObject *exception = nullptr;

        mono_runtime_invoke(scriptData->MethodOnStart, objectHandle->Object, nullptr, &exception);

        if (exception != nullptr)
        {
            auto str = mono_object_to_string(exception, nullptr);

            PError(fmt::format("Exception thrown in script '{}': {}", script->GetFilePath().string(), mono_string_to_utf8(str)));
        }
    }
}

void Pine::Script::Manager::OnUpdate(float deltaTime)
{
    PINE_PF_SCOPE();

    for (auto& scriptComponent : Components::Get<ScriptComponent>())
    {
        auto script = scriptComponent.GetScript();

        if (!script)
        {
            continue;
        }

        auto scriptData = script->GetScriptData();

        if (!scriptData || !scriptData->IsReady || !scriptData->MethodOnUpdate)
        {
            continue;
        }

        auto objectHandle = scriptComponent.GetScriptObjectHandle();
        if (!objectHandle || objectHandle->Object == nullptr)
        {
            continue;
        }

        void* args[1] = { &deltaTime };
        MonoObject *exception = nullptr;

        mono_runtime_invoke(scriptData->MethodOnUpdate, objectHandle->Object, args, &exception);

        if (exception != nullptr)
        {
            auto str = mono_object_to_string(exception, nullptr);

            PError(fmt::format("Exception thrown in script '{}': {}", script->GetFilePath().string(), mono_string_to_utf8(str)));
        }
    }
}

void Pine::Script::Manager::OnRender(float deltaTime)
{
}