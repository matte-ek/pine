#pragma once
#include "Pine/World/Components/Component/Component.hpp"
#include <memory>

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

        // A component's SaveData() only covers the fields of its own type, so the active flag has
        // to be captured next to it - otherwise undoing "disable component" restores identical data
        // and appears to do nothing.
        bool m_PreActive = true;
        bool m_PostActive = true;
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

        // See the note in UpdateComponentCommand: a re-created component would come back enabled.
        bool m_ComponentActive = true;
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

    // Set by every command created anywhere this frame, cleared once in Update(). A consumer that wants
    // to know whether its *own* section of UI changed something has to ClearItemUpdated() first, or it
    // also sees unrelated commands made earlier in the frame (the viewport gizmo, for instance).
    bool HasItemUpdated();
    void ClearItemUpdated();

    struct HistoryState
    {
        std::size_t UndoCount = 0;
        std::size_t RedoCount = 0;
    };

    struct HistoryResult
    {
        bool Applied = false;
        std::string Error;
    };

    // Finish any held UI edit before an external batch samples its pre-state.
    void FinishHeldCommand();
    void RegisterCommand(std::unique_ptr<EditorCommand> command);
    void ClearHistory();
    HistoryState GetHistoryState();

    HistoryResult ExecuteUndo();
    HistoryResult ExecuteRedo();

    void Setup();
    void Update();
    void Shutdown();
}
