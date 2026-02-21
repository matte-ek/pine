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

        /*
        for (auto& [path, script] : Pine::Assets::GetAll())
        {
            if (script->GetType() != Pine::AssetType::CSharpScript)
            {
                continue;
            }

            if (script->IsDeleted())
            {
                continue;
            }

            scripts.push_back(dynamic_cast<Pine::CSharpScript*>(script));
        }
        */

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

            scriptData->Fields.push_back(new Pine::ScriptField(name, field, scriptData, mono_type_get_name(type)));
        }
    }

    // Populates all fields of a script data instance
    void ResolveScriptData(Pine::ScriptData* scriptData)
    {
        const auto fileName = scriptData->Asset->GetFilePath().stem().string();

        if (!m_GameAssembly)
        {
            return;
        }

        // Currently, the class name has to be the same as the file name, maybe finding the first class that
        // matches our requirements in a file is a better solution? Also for the future, a custom namespace might be nice.
        auto monoClass = mono_class_from_name(m_GameAssembly->Image, "Game", fileName.c_str());
        if (!monoClass)
        {
            Pine::Log::Warning(fmt::format("Failed to find class for script: {}", fileName));
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
    if (Runtime::GetPineAssembly() == nullptr)
    {
        // If we've failed to load the entire Pine runtime, there is no need to be looking for
        // the game runtime, as things won't work anyway.
        return;
    }

    // Attempt to find the game runtime at the default location
    if (!std::filesystem::exists("game/runtime-bin/Game.dll"))
    {
        Log::Error("Script: Failed to find game runtime, project might be missing scripts?");
        return;
    }

    LoadGameAssembly("game/runtime-bin/Game.dll");
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
    m_GameAssembly = Script::Runtime::LoadAssembly(path);

    if (!m_GameAssembly)
    {
        Log::Error("Failed to load game assembly.");
        return;
    }

    m_GameAssemblyPath = path;
    m_HasGameAssembly = true;
}

void Pine::Script::Manager::ReloadGameAssembly()
{
    assert(m_HasGameAssembly);

    Script::Runtime::Reset();

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

    for (auto& scriptComponent : Components::Get<Pine::ScriptComponent>())
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

            Log::Error(fmt::format("Exception thrown in script '{}': {}", script->GetFilePath().string(), mono_string_to_utf8(str)));
        }
    }
}

void Pine::Script::Manager::OnUpdate(float deltaTime)
{
    PINE_PF_SCOPE();

    for (auto& scriptComponent : Components::Get<Pine::ScriptComponent>())
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

            Log::Error(fmt::format("Exception thrown in script '{}': {}", script->GetFilePath().string(), mono_string_to_utf8(str)));
        }
    }
}

void Pine::Script::Manager::OnRender(float deltaTime)
{
}