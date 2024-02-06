#include "ScriptManager.hpp"
#include "Pine/Script/Runtime/ScriptingRuntime.hpp"
#include "Pine/Core/Log/Log.hpp"
#include "Pine/Script/Scripts/ScriptData.hpp"
#include "Pine/Assets/CSharpScript/CSharpScript.hpp"
#include "Pine/Assets/Assets.hpp"
#include "Pine/Script/Factory/ScriptObjectFactory.hpp"
#include "Pine/World/Components/Components.hpp"
#include "Pine/World/Components/Script/ScriptComponent.hpp"
#include "Pine/World/Entity/Entity.hpp"

#include <vector>

#include <mono/metadata/appdomain.h>

namespace
{
    bool m_HasGameAssembly = false;

    Pine::Script::RuntimeAssembly* m_GameAssembly;
    std::filesystem::path m_GameAssemblyPath;

    std::vector<Pine::ScriptData*> m_ScriptData;

    std::vector<Pine::CSharpScript*> GetScripts()
    {
        std::vector<Pine::CSharpScript*> scripts;

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

        return scripts;
    }

    void ResolveScriptData(Pine::ScriptData* scriptData)
    {
        const auto fileName = scriptData->Asset->GetFilePath().stem().string();

        auto monoClass = mono_class_from_name(m_GameAssembly->Image, "Game", fileName.c_str());
        if (!monoClass)
        {
            Pine::Log::Warning(fmt::format("Failed to find class for script: {}", fileName));
            return;
        }

        scriptData->Class = monoClass;

        scriptData->MethodOnStart = mono_class_get_method_from_name(scriptData->Class, "OnStart", 0);
        scriptData->MethodOnDestroy = mono_class_get_method_from_name(scriptData->Class, "OnDestroy", 0);
        scriptData->MethodOnUpdate = mono_class_get_method_from_name(scriptData->Class, "OnUpdate", 0);
        scriptData->MethodOnRender = mono_class_get_method_from_name(scriptData->Class, "OnRender", 0);
        scriptData->FieldEntity = mono_class_get_field_from_name(scriptData->Class, "Parent");

        scriptData->IsReady = monoClass && scriptData->FieldEntity;
    }
}

void Pine::Script::Manager::Setup()
{
    if (!Runtime::Setup())
    {
        // We should already be logging out relevant information in the Runtime::Setup function.
        return;
    }

    ObjectFactory::Setup();

    // Attempt to find the game runtime at the default location
    if (!std::filesystem::exists("game/runtime-bin/Game.dll"))
    {
        Log::Error("Failed to find game runtime at default location.");
        return;
    }

    LoadGameAssembly("game/runtime-bin/Game.dll");
}

void Pine::Script::Manager::Dispose()
{
    Runtime::Dispose();
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

    if (!Script::Runtime::UnloadAssembly(m_GameAssembly))
    {
        Log::Error("Failed to unload game assembly.");
        return;
    }

    LoadGameAssembly(m_GameAssemblyPath);
    ReloadScripts();
}

void Pine::Script::Manager::ReloadScripts()
{
    for (const auto& script : m_ScriptData)
    {
        script->Asset->SetScriptData(nullptr);

        delete script;
    }

    m_ScriptData.clear();

    const auto& scripts = GetScripts();

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
    for (auto& scriptComponent : Components::Get<Pine::ScriptComponent>())
    {
        auto script = scriptComponent.GetScript();

        if (!script)
        {
            continue;
        }

        auto scriptData = script->GetScriptData();

        if (!scriptData || !scriptData->IsReady)
        {
            continue;
        }

        auto objectHandle = scriptComponent.GetScriptObjectHandle();
        if (!objectHandle || objectHandle->Object == nullptr)
        {
            continue;
        }

        mono_runtime_invoke(scriptData->MethodOnStart, objectHandle->Object, nullptr, nullptr);
    }
}

void Pine::Script::Manager::OnUpdate(float deltaTime)
{

}

void Pine::Script::Manager::OnRender(float deltaTime)
{
}
