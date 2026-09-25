#include "ComponentClipboard.hpp"

#include "Other/Actions/Actions.hpp"
#include "Pine/World/Components/Components.hpp"

namespace
{
    bool m_HasData = false;

    Pine::ComponentType m_Type = Pine::ComponentType::Transform;
    Pine::ByteSpan m_Data;
}

void Editor::Clipboard::Component::Copy(Pine::Component* component)
{
    m_Type = component->GetType();
    m_Data = component->SaveData();
    m_HasData = true;
}

bool Editor::Clipboard::Component::HasData()
{
    return m_HasData;
}

Pine::ComponentType Editor::Clipboard::Component::GetType()
{
    return m_Type;
}

bool Editor::Clipboard::Component::CanPasteValues(const Pine::Component* component)
{
    return m_HasData && component->GetType() == m_Type;
}

bool Editor::Clipboard::Component::CanPasteAsNew()
{
    if (!m_HasData)
    {
        return false;
    }

    // The same two types the "Add new component" picker leaves out: an entity has exactly one
    // Transform, and native scripts are not created from the editor.
    if (m_Type == Pine::ComponentType::Transform || m_Type == Pine::ComponentType::NativeScript)
    {
        return false;
    }

    return Pine::Components::GetFreeSlotCount(m_Type) > 0;
}

void Editor::Clipboard::Component::PasteValues(Pine::Component* component)
{
    if (!CanPasteValues(component))
    {
        return;
    }

    Actions::CreateComponentCommand updateCmd(component, Actions::CommandType::Update);

    component->LoadData(m_Data);
}

Pine::Component* Editor::Clipboard::Component::PasteAsNew(Pine::Entity* entity)
{
    if (!CanPasteAsNew())
    {
        return nullptr;
    }

    const auto component = entity->AddComponent(m_Type);

    component->LoadData(m_Data);

    // A create command captures the component's data when it is constructed, so it has to come
    // after the load for redo to bring back the pasted values.
    Actions::CreateComponentCommand(component, Actions::CommandType::Create);

    return component;
}
