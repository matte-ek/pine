#include "Actions.hpp"

#include <memory>
#include <vector>

#include "imgui.h"
#include "Other/PlayHandler/PlayHandler.hpp"

#include "Pine/Core/Serialization/Serialization.hpp"
#include "Pine/World/Components/Components.hpp"
#include "Pine/World/Entities/Entities.hpp"
#include "Pine/World/Entity/Entity.hpp"

namespace
{
    using namespace Editor::Actions;

    constexpr std::size_t MaxHistorySize = 128;

    bool m_ItemUpdated = false;

    bool m_IsSavingHeldState = false;
    std::unique_ptr<EditorCommand> m_HeldStateCommand;
    std::uint64_t m_HistoryGeneration = 0;

    // Standard undo stack: index 0 is the oldest command, the back is the newest. m_CommandIndex is the number
    // of commands currently "applied" - the next undo targets m_CommandHistory[m_CommandIndex - 1], the next
    // redo targets m_CommandHistory[m_CommandIndex]. Ownership lives in the unique_ptrs, so trimming the history
    // frees the commands automatically.
    std::size_t m_CommandIndex = 0;
    std::vector<std::unique_ptr<EditorCommand>> m_CommandHistory;

    void SynchronizeScene()
    {
        if (m_HistoryGeneration != Pine::Entities::GetSceneGeneration())
        {
            ClearHistory();
        }
    }

    void AppendCommand(std::unique_ptr<EditorCommand> editorCommand)
    {
        // Anything that was undone can no longer be redone once new history is written, so discard it.
        if (m_CommandIndex < m_CommandHistory.size())
        {
            PVerbose(fmt::format("Re-writing history, discarding {} redoable command(s)", m_CommandHistory.size() - m_CommandIndex));

            m_CommandHistory.erase(m_CommandHistory.begin() + m_CommandIndex, m_CommandHistory.end());
        }

        m_CommandHistory.push_back(std::move(editorCommand));
        m_CommandIndex = m_CommandHistory.size();

        if (m_CommandHistory.size() > MaxHistorySize)
        {
            m_CommandHistory.erase(m_CommandHistory.begin());
            m_CommandIndex--;
        }

        PVerbose(fmt::format("Writing history, index: {}, size: {}", m_CommandIndex, m_CommandHistory.size()));
    }
}

void EditorCommand::SaveState(CommandState commandState)
{
}

UpdateComponentCommand::UpdateComponentCommand(const Pine::Component* component, const bool savePreState)
{
    m_ComponentId = component->GetId();
    m_ComponentType = component->GetType();

    if (savePreState)
    {
        UpdateComponentCommand::SaveState(CommandState::PreCommand);
    }
}

void UpdateComponentCommand::SaveState(const CommandState commandState)
{
    const auto component = Pine::Components::FindById(m_ComponentType, m_ComponentId);

    if (component == nullptr)
    {
        PWarning(fmt::format("UpdateComponentCommand::SaveState: component {} no longer exists, skipping.", m_ComponentId.ToString()));
        return;
    }

    if (commandState == CommandState::PreCommand)
    {
        m_PreCommand = component->SaveData();
        m_PreActive = component->GetActive();
    }
    else
    {
        m_PostCommand = component->SaveData();
        m_PostActive = component->GetActive();
    }
}

void UpdateComponentCommand::Apply(const CommandState commandState)
{
    const auto component = Pine::Components::FindById(m_ComponentType, m_ComponentId);

    if (component == nullptr)
    {
        PWarning(fmt::format("UpdateComponentCommand::Apply: component {} no longer exists, skipping.", m_ComponentId.ToString()));
        return;
    }

    const bool isPreCommand = commandState == CommandState::PreCommand;

    component->LoadData(isPreCommand ? m_PreCommand : m_PostCommand);
    component->SetActive(isPreCommand ? m_PreActive : m_PostActive);
}

CreateDeleteComponentCommand::CreateDeleteComponentCommand(Pine::Component* component, const CommandType type)
    : m_CommandType(type)
{
    m_ComponentId = component->GetId();
    m_ParentId = component->GetParent()->GetId();

    m_ComponentType = component->GetType();
    m_ComponentData = component->SaveData();
    m_ComponentActive = component->GetActive();
}

void CreateDeleteComponentCommand::Apply(const CommandState commandState)
{
    if ((commandState == CommandState::PreCommand && m_CommandType == CommandType::Delete) ||
        (commandState == CommandState::PostCommand && m_CommandType == CommandType::Create))
    {
        const auto entity = Pine::Entities::Find(m_ParentId);

        if (entity == nullptr)
        {
            PWarning(fmt::format("CreateDeleteComponentCommand::Apply: parent entity {} no longer exists, skipping.", m_ParentId.ToString()));
            return;
        }

        const auto component = entity->AddComponent(m_ComponentType);

        component->SetId(m_ComponentId);
        component->LoadData(m_ComponentData);
        component->SetActive(m_ComponentActive);
    }
    else if ((commandState == CommandState::PostCommand && m_CommandType == CommandType::Delete) ||
             (commandState == CommandState::PreCommand && m_CommandType == CommandType::Create))
    {
        const auto component = Pine::Components::FindById(m_ComponentType, m_ComponentId);

        if (component == nullptr)
        {
            PWarning(fmt::format("CreateDeleteComponentCommand::Apply: component {} no longer exists, skipping.", m_ComponentId.ToString()));
            return;
        }

        component->GetParent()->RemoveComponent(component);
    }
}

CreateComponentCommand::CreateComponentCommand(Pine::Component* component, CommandType type, bool isDragEvent)
{
    auto CreateCommand = [component, type]() -> EditorCommand*
    {
        switch (type)
        {
            case CommandType::Update:
                return new UpdateComponentCommand(component);
            case CommandType::Create:
            case CommandType::Delete:
                return new CreateDeleteComponentCommand(component, type);
            default:
                return nullptr;
        }
    };

    m_ItemUpdated = true;

    // For most controls, this code will get ran on mouse release (for example clicking buttons/checkboxes/dropdowns)
    // however for sliders, this code may get ran on every small step, and would therefore spam the action system with
    // mini-changes while the user would only want to see the start and the finish of the operation. As such, we
    // add this code to detect this state and handle it accordingly.

    if (ImGui::IsMouseDown(ImGuiMouseButton_Left))
    {
        if (!m_IsSavingHeldState)
        {
            m_IsSavingHeldState = true;
            m_HeldStateCommand.reset(CreateCommand());
        }

        return;
    }

    // This is really annoying but for some reason I can't understand ImGuizmo decides to continue sending commands
    // even after releasing the mouse button, so we have this "hack" in place to stop that from happening.
    if (isDragEvent)
    {
        return;
    }

    m_Command = CreateCommand();
}

CreateComponentCommand::~CreateComponentCommand()
{
    if (m_Command == nullptr)
    {
        return;
    }

    // The caller mutates the component between constructing this scope object and it going out of scope, so now
    // is the moment to snapshot the resulting ("post") state - otherwise redo would have nothing to restore.
    // (For create/delete commands SaveState is a no-op; their data is captured in the constructor.)
    m_Command->SaveState(CommandState::PostCommand);

    RegisterCommand(std::unique_ptr<EditorCommand>(m_Command));
}

bool Editor::Actions::HasItemUpdated()
{
    return m_ItemUpdated;
}

void Editor::Actions::ClearItemUpdated()
{
    m_ItemUpdated = false;
}

void Editor::Actions::ClearHistory()
{
    m_CommandHistory.clear();
    m_CommandIndex = 0;

    m_HeldStateCommand.reset();
    m_IsSavingHeldState = false;

    m_HistoryGeneration = Pine::Entities::GetSceneGeneration();
}

void Editor::Actions::FinishHeldCommand()
{
    SynchronizeScene();

    if (m_HeldStateCommand != nullptr)
    {
        m_HeldStateCommand->SaveState(CommandState::PostCommand);

        AppendCommand(std::move(m_HeldStateCommand));
    }

    m_IsSavingHeldState = false;
}

void Editor::Actions::RegisterCommand(std::unique_ptr<EditorCommand> command)
{
    FinishHeldCommand();
    AppendCommand(std::move(command));
}

Editor::Actions::HistoryState Editor::Actions::GetHistoryState()
{
    SynchronizeScene();
    return { m_CommandIndex, m_CommandHistory.size() - m_CommandIndex };
}

namespace
{
    HistoryResult ApplyHistory(const bool redo)
    {
        if (PlayHandler::GetGameState() != PlayHandler::EditorGameState::Stopped)
        {
            PWarning("Stop play mode before undo or redo.");
            return { false, "Stop play mode before undo or redo." };
        }

        try
        {
            FinishHeldCommand();

            if (redo ? m_CommandIndex == m_CommandHistory.size() : m_CommandIndex == 0)
            {
                return {};
            }

            const auto index = redo ? m_CommandIndex : m_CommandIndex - 1;

            m_CommandHistory[index]->Apply(redo ? CommandState::PostCommand : CommandState::PreCommand);

            m_CommandIndex = redo ? index + 1 : index;

            return { true, {} };
        }
        catch (const std::exception& exception)
        {
            // Application may have stopped midway. Do not offer earlier commands against that state.
            const std::string error = exception.what();
            ClearHistory();
            PError(fmt::format("History restoration failed; history cleared: {}", error));
            return { false, error };
        }
    }
}

Editor::Actions::HistoryResult Editor::Actions::ExecuteUndo()
{
    return ApplyHistory(false);
}

Editor::Actions::HistoryResult Editor::Actions::ExecuteRedo()
{
    return ApplyHistory(true);
}

void Editor::Actions::Update()
{
    SynchronizeScene();

    if (m_IsSavingHeldState && ImGui::IsMouseReleased(ImGuiMouseButton_Left))
    {
        FinishHeldCommand();
    }

    m_ItemUpdated = false;
}

void Editor::Actions::Setup()
{
    ClearHistory();
}

void Editor::Actions::Shutdown()
{
    ClearHistory();
}
