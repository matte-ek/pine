#pragma once
#include "Pine/World/Components/Component/Component.hpp"

namespace Editor::Actions
{
    enum ActionType
    {
        Invalid,
        Entity,
        Component,
        Asset
    };

    enum class CommandType
    {
        Create,
        Update,
        Delete
    };

    enum class CommandState
    {
        PreCommand,
        PostCommand
    };

    class EditorCommand
    {
    private:
    public:
        virtual ~EditorCommand() = default;

        virtual void SaveState(CommandState commandState);
        virtual void Apply(CommandState commandState) = 0;
    };

    // Handles the updating of a component state forwards/backwards
    class UpdateComponentCommand : public EditorCommand
    {
    private:
        Pine::ComponentType m_ComponentType;
        Pine::UId m_ComponentId;

        Pine::ByteSpan m_PreCommand;
        Pine::ByteSpan m_PostCommand;
    public:
        explicit UpdateComponentCommand(const Pine::Component* component, bool savePreState = true);

        void SaveState(CommandState commandState) override;
        void Apply(CommandState commandState) override;
    };

    // Handles the creation and deletion of a component
    class CreateDeleteComponentCommand : public EditorCommand
    {
    private:
        CommandType m_CommandType;

        Pine::ComponentType m_ComponentType;

        Pine::UId m_ParentId;
        Pine::UId m_ComponentId;

        Pine::ByteSpan m_ComponentData;
    public:
        explicit CreateDeleteComponentCommand(Pine::Component* component, CommandType type);
        void Apply(CommandState commandState) override;
    };

    class UpdateEntityCommand : public EditorCommand
    {
    private:
    public:
    };

    class CreateDeleteEntityCommand : public EditorCommand
    {
    private:
        Pine::UId m_EntityId;
    public:
    };

    // RAII wrappers to ease command creation

    class CreateComponentCommand
    {
    private:
        EditorCommand* m_Command = nullptr;
    public:
        explicit CreateComponentCommand(Pine::Component* component, CommandType type, bool isDragEvent = false);
        ~CreateComponentCommand();
    };

    bool HasItemUpdated();

    void ExecuteUndo();
    void ExecuteRedo();

    void Setup();
    void Update();
    void Shutdown();
}
