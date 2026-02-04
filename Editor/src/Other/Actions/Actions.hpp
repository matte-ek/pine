#pragma once
#include "Pine/World/Components/Component/Component.hpp"

namespace Actions
{
    enum ActionType
    {
        Invalid,
        Entity,
        Component,
        Asset
    };

    enum ActionEvent
    {
        Create,
        Delete,
        Modify
    };

    class Action
    {
    protected:
        ActionType m_Type = Invalid;
        ActionEvent m_Event = Create;
    public:
        virtual ~Action() = default;

        ActionType GetActionType() const;

        void SetEvent(ActionEvent event);
        ActionEvent GetEvent() const;

        virtual void Apply() = 0;
    };

    class ActionEntity : public Action
    {
    public:
        ActionEntity();

        void Apply() override;
    };

    class ActionComponent : public Action
    {
    private:
        Pine::ComponentType m_ComponentType;
        std::uint32_t m_Index;
        nlohmann::json m_Data;
    public:
        ActionComponent();

        void Store(Pine::Component* component);

        void Apply() override;
    };

    void RegisterComponentAction(ActionEvent event, Pine::Component* component);

    void Undo();
    void Redo();

    void Setup();
    void Shutdown();
}
