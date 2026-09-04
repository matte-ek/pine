#include "Actions.hpp"

#include <memory>
#include <vector>

#include "imgui.h"

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
    EditorCommand* m_HeldStateCommand = nullptr;

    // Standard undo stack: index 0 is the oldest command, the back is the newest. m_CommandIndex is the number
    // of commands currently "applied" - the next undo targets m_CommandHistory[m_CommandIndex - 1], the next
    // redo targets m_CommandHistory[m_CommandIndex]. Ownership lives in the unique_ptrs, so trimming the history
    // frees the commands automatically.
    std::size_t m_CommandIndex = 0;
    std::vector<std::unique_ptr<EditorCommand>> m_CommandHistory;

    void RegisterCommand(EditorCommand* editorCommand)
    {
        // Anything that was undone can no longer be redone once new history is written, so discard it.
        if (m_CommandIndex < m_CommandHistory.size())
        {
            PVerbose(fmt::format("Re-writing history, discarding {} redoable command(s)", m_CommandHistory.size() - m_CommandIndex));

            m_CommandHistory.erase(m_CommandHistory.begin() + m_CommandIndex, m_CommandHistory.end());
        }

        m_CommandHistory.emplace_back(editorCommand);
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

    (commandState == CommandState::PreCommand ? m_PreCommand : m_PostCommand) = component->SaveData();
}

void UpdateComponentCommand::Apply(const CommandState commandState)
{
    const auto component = Pine::Components::FindById(m_ComponentType, m_ComponentId);

    if (component == nullptr)
    {
        PWarning(fmt::format("UpdateComponentCommand::Apply: component {} no longer exists, skipping.", m_ComponentId.ToString()));
        return;
    }

    component->LoadData(commandState == CommandState::PreCommand ? m_PreCommand : m_PostCommand);
}

CreateDeleteComponentCommand::CreateDeleteComponentCommand(Pine::Component* component, const CommandType type)
    : m_CommandType(type)
{
    m_ComponentId = component->GetId();
    m_ParentId = component->GetParent()->GetId();

    m_ComponentType = component->GetType();
    m_ComponentData = component->SaveData();
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
            m_HeldStateCommand = CreateCommand();
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

    RegisterCommand(m_Command);
}

bool Editor::Actions::HasItemUpdated()
{
    return m_ItemUpdated;
}

void Editor::Actions::ClearItemUpdated()
{
    m_ItemUpdated = false;
}

void Editor::Actions::ExecuteUndo()
{
    if (m_CommandIndex == 0)
    {
        return;
    }

    PVerbose(fmt::format("Executing undo, index: {}, size: {}", m_CommandIndex, m_CommandHistory.size()));

    m_CommandIndex--;
    m_CommandHistory[m_CommandIndex]->Apply(CommandState::PreCommand);
}

void Editor::Actions::ExecuteRedo()
{
    if (m_CommandIndex >= m_CommandHistory.size())
    {
        return;
    }

    PVerbose(fmt::format("Executing redo, index: {}, size: {}", m_CommandIndex, m_CommandHistory.size()));

    m_CommandHistory[m_CommandIndex]->Apply(CommandState::PostCommand);
    m_CommandIndex++;
}

void Editor::Actions::Update()
{
    if (m_IsSavingHeldState && ImGui::IsMouseReleased(ImGuiMouseButton_Left))
    {
        m_IsSavingHeldState = false;
        m_HeldStateCommand->SaveState(CommandState::PostCommand);

        RegisterCommand(m_HeldStateCommand);
    }

    m_ItemUpdated = false;
}
