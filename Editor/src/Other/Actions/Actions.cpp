#include "Actions.hpp"

#include "Pine/World/Components/Components.hpp"

namespace
{
    using namespace Actions;

    std::vector<Action*> m_RecentActions;

    bool m_RestoreCursor = false;
    std::uint64_t m_ActionCursor = 0;

    void StoreAction(Action* action)
    {
        if (m_RecentActions.size() > 128)
        {
            delete m_RecentActions[0];
            m_RecentActions.erase(m_RecentActions.begin());
        }

        m_RecentActions.push_back(action);

        if (m_RestoreCursor)
        {
            m_ActionCursor = m_RecentActions.size() - 1;
            m_RestoreCursor = false;
        }
    }

    void ClearActions()
    {
        for (auto action : m_RecentActions)
        {
            delete action;
        }

        m_RecentActions.clear();
    }
}

ActionType Action::GetActionType() const
{
    return m_Type;
}

void Action::SetEvent(ActionEvent event)
{
    m_Event = event;
}

ActionEvent Action::GetEvent() const
{
    return m_Event;
}

ActionEntity::ActionEntity()
{
    m_Type = Entity;
}

void ActionEntity::Apply()
{
}

ActionComponent::ActionComponent() :
    m_ComponentType(Pine::ComponentType::Transform),
    m_Index(0)
{
    m_Type = Component;
}

void ActionComponent::Store(Pine::Component* component)
{
    m_Index = component->GetInternalId();
    m_ComponentType = component->GetType();
}

void ActionComponent::Apply()
{
    auto component = Pine::Components::GetByInternalId(m_ComponentType, m_Index);

    if (!component)
    {
        return;
    }
}

void Actions::RegisterComponentAction(ActionEvent event, Pine::Component* component)
{
    auto action = new ActionComponent();

    action->SetEvent(event);
    action->Store(component);

    StoreAction(action);
}

void Actions::Undo()
{
    if (m_RecentActions.empty())
    {
        return;
    }

    if (m_ActionCursor > 0)
    {
        m_ActionCursor--;
    }

    m_RestoreCursor = true;

    if (m_ActionCursor < m_RecentActions.size())
    {
        auto action = m_RecentActions[m_ActionCursor];
        action->Apply();
    }
}

void Actions::Redo()
{
}

void Actions::Setup()
{
    m_RecentActions.reserve(512);
}

void Actions::Shutdown()
{
}
