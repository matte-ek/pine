#include "ScriptingUtilities.hpp"
#include "Pine/Core/WindowManager/WindowManager.hpp"
#include "Pine/Script/ScriptManager.hpp"
#include "Pine/Core/Log/Log.hpp"
#include "Other/PlayHandler/PlayHandler.hpp"
#include "Pine/Rendering/RenderManager/RenderManager.hpp"
#include <filesystem>

#include "mono/metadata/class.h"
#include "mono/metadata/object.h"
#include "Pine/Script/Runtime/ScriptingRuntime.hpp"

namespace
{
    constexpr auto GameAssemblyPath = "game/runtime-bin/Game.dll";

    std::filesystem::file_time_type m_LastGameAssemblyWriteTime;
    bool m_ReloadGameAssemblyRequest = false;

    bool PollGameAssemblyWriteTime()
    {
        if (!std::filesystem::exists(GameAssemblyPath))
            return false;

        const auto writeTime = std::filesystem::last_write_time(GameAssemblyPath);

        if (writeTime != m_LastGameAssemblyWriteTime)
        {
            m_LastGameAssemblyWriteTime = writeTime;

            return true;
        }

        return false;
    }

    void OnWindowFocus()
    {
        if (PlayHandler::GetGameState() != PlayHandler::EditorGameState::Stopped)
            return;
        if (!PollGameAssemblyWriteTime())
            return;

        m_ReloadGameAssemblyRequest = true;
    }

    void OnRender(Pine::RenderingContext*, Pine::RenderStage stage, float)
    {
        if (stage != Pine::RenderStage::PostRender)
            return;

        if (m_ReloadGameAssemblyRequest)
        {
            Pine::Log::Info("Reloading updated game assembly...");

            Pine::Script::Manager::ReloadGameAssembly();

            m_ReloadGameAssemblyRequest = false;
        }
    }

    void ManageScriptFile(const std::string& filePath, bool deleteFile)
    {
        auto editorUtilitiesClass = mono_class_from_name(Pine::Script::Runtime::GetPineImage(), "Pine.Core", "EditorUtils");
        if (!editorUtilitiesClass)
        {
            Pine::Log::Warning(fmt::format("Could not find the EditorUtils class from the Pine runtime."));
            return;
        }

        auto scriptMethodName = deleteFile ? "RemoveScriptFile" : "AddScriptFile";

        auto scriptMethod = mono_class_get_method_from_name(editorUtilitiesClass, scriptMethodName, 1);
        if (!scriptMethod)
        {
            Pine::Log::Warning(fmt::format("Could not find the {} method from the Pine runtime.", scriptMethodName));
            return;
        }

        MonoObject *exception = nullptr;

        auto managedPathStr = mono_string_new(Pine::Script::Runtime::GetDomain(), filePath.c_str());
        void* args[1] = { managedPathStr };

        mono_runtime_invoke(scriptMethod, nullptr, args, &exception);

        if (exception != nullptr)
        {
            auto str = mono_object_to_string(exception, nullptr);
            Pine::Log::Error(fmt::format("Exception thrown in script '{}': {}", "EditorUtils.cs", mono_string_to_utf8(str)));
        }
    }

}

void ScriptingUtilities::Setup()
{
    Pine::WindowManager::AddWindowFocusCallback(OnWindowFocus);
    Pine::RenderManager::AddRenderCallback(OnRender);

    PollGameAssemblyWriteTime();
}

void ScriptingUtilities::AddScript(const std::string& filePath)
{
    ManageScriptFile(filePath, false);
}

void ScriptingUtilities::DeleteScript(const std::string& filePath)
{
    ManageScriptFile(filePath, true);
}
