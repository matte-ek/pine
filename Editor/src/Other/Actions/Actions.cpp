#include "Actions.hpp"

#include "imgui.h"

#include "Pine/Core/Serialization/Serialization.hpp"
#include "Pine/World/Components/Components.hpp"
#include "Pine/World/Entities/Entities.hpp"
#include "Pine/World/Entity/Entity.hpp"

namespace
{
    using namespace Editor::Actions;

    bool m_ItemUpdated = false;

    bool m_IsSavingHeldState = false;
    EditorCommand* m_HeldStateCommand = nullptr;

    int m_CommandPointer = 0;
    std::deque<EditorCommand*> m_CommandHistory;

    void RegisterCommand(EditorCommand* editorCommand)
    {
        // We're going to rewrite history
        if (m_CommandPointer > 0)
        {
            PInfo(fmt::format("Re-writing history, current pointer: {}, current size: {}", m_CommandPointer, m_CommandHistory.size()));

            for (int i = m_CommandPointer - 1; i >= 0; --i)
            {
                delete m_CommandHistory[i];
                m_CommandHistory.erase(m_CommandHistory.begin() + i);
            }

            m_CommandPointer = 0;
        }

        m_CommandHistory.push_front(editorCommand);

        if (m_CommandHistory.size() > 128)
        {
            delete m_CommandHistory[m_CommandHistory.size() - 1];
            m_CommandHistory.erase(m_CommandHistory.end());
        }

        PInfo(fmt::format("Writing history, ptr: {}, size: {}", m_CommandPointer, m_CommandHistory.size()));
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

    assert(component != nullptr);

    (commandState == CommandState::PreCommand ? m_PreCommand : m_PostCommand) = component->SaveData();
}

void UpdateComponentCommand::Apply(const CommandState commandState)
{
    const auto component = Pine::Components::FindById(m_ComponentType, m_ComponentId);

    assert(component != nullptr);

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

        assert(entity != nullptr);

        const auto component = entity->AddComponent(m_ComponentType);

        component->SetId(m_ComponentId);
        component->LoadData(m_ComponentData);
    }
    else if ((commandState == CommandState::PostCommand && m_CommandType == CommandType::Delete) ||
             (commandState == CommandState::PreCommand && m_CommandType == CommandType::Create))
    {
        const auto component = Pine::Components::FindById(m_ComponentType, m_ComponentId);

        assert(component != nullptr);

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

    RegisterCommand(m_Command);
}

bool Editor::Actions::HasItemUpdated()
{
    return m_ItemUpdated;
}

void Editor::Actions::ExecuteUndo()
{
    if (m_CommandPointer >= m_CommandHistory.size())
    {
        return;
    }

    PInfo(fmt::format("Executing undo, ptr: {}, size: {}", m_CommandPointer, m_CommandHistory.size()));

    m_CommandHistory[m_CommandPointer]->Apply(CommandState::PreCommand);
    m_CommandPointer++;
}

void Editor::Actions::ExecuteRedo()
{
    if (m_CommandHistory.empty() || m_CommandPointer == 0)
    {
        return;
    }

    PInfo(fmt::format("Executing redo, ptr: {}, size: {}", m_CommandPointer, m_CommandHistory.size()));

    m_CommandPointer--;
    m_CommandHistory[m_CommandPointer]->Apply(CommandState::PostCommand);
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
